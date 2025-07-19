#include "HydraEngine/Base.h"
#include "HRay/Icons.h"

import HE;
import Assets;
import ImGui;
import Editor;
import std;

#pragma region ViewPortWindow


void Editor::ViewPortWindow::OnCreate()
{
    Editor::GetWindowDesc(this).menuBar = true;

    orbitCamera = HE::CreateScope<Editor::OrbitCamera>(60.0f, float(width) / float(height), 0.1f, 1000.0f);
    flyCamera = HE::CreateScope<Editor::FlyCamera>(60.0f, float(width) / float(height), 0.1f, 1000.0f);
    editorCamera = orbitCamera;
}

void Editor::ViewPortWindow::OnUpdate(HE::Timestep ts)
{
    auto& ctx = GetContext();
    Assets::Scene* scene = ctx.assetManager.GetAsset<Assets::Scene>(ctx.sceneHandle);

    {
        HE_PROFILE_SCOPE("Render");

        if (scene)
        {
            Assets::Entity mainCameraEntity = Editor::GetSceneCamera(scene);

            HRay::BeginScene(ctx.rd, fd);

            {
                auto view = scene->registry.view<Assets::MeshComponent>();
                for (auto e : view)
                {
                    Assets::Entity entity = { e, scene };
                    auto& dm = entity.GetComponent<Assets::MeshComponent>();
                    auto wt = entity.GetWorldSpaceTransformMatrix();

                    auto asset = ctx.assetManager.GetAsset(dm.meshSourceHandle);
                    if (asset && asset.Has<Assets::MeshSource>() && asset.GetState() == Assets::AssetState::Loaded)
                    {
                        auto& meshSource = asset.Get<Assets::MeshSource>();
                        auto& mesh = meshSource.meshes[dm.meshIndex];

                        HRay::SubmitMesh(ctx.rd, fd, asset, mesh, wt, ctx.commandList);
                    }
                }
            }

            {
                auto view = scene->registry.view<Assets::DirectionalLightComponent>();
                for (auto e : view)
                {
                    Assets::Entity entity = { e, scene };
                    auto& light = entity.GetComponent<Assets::DirectionalLightComponent>();
                    auto wt = entity.GetWorldSpaceTransformMatrix();

                    HRay::SubmitDirectionalLight(ctx.rd, fd, light, wt);
                }
            }

            {
                auto view = scene->registry.view<Assets::DynamicSkyLightComponent>();
                for (auto e : view)
                {
                    Assets::Entity entity = { e, scene };
                    auto& dynamicSkyLight = entity.GetComponent<Assets::DynamicSkyLightComponent>();

                    HRay::SubmitSkyLight(ctx.rd, fd, dynamicSkyLight);
                }

                fd.sceneInfo.light.enableEnvironmentLight = view.size();
            }

            if (previewMode && mainCameraEntity && cameraAnimation.state & Animation::None)
            {
                auto& c = mainCameraEntity.GetComponent<Assets::CameraComponent>();
                auto wt = mainCameraEntity.GetWorldSpaceTransformMatrix();
                Math::float4x4 viewMatrix = Math::inverse(wt);
                float fov = c.perspectiveFieldOfView;

                Math::vec3 camPos, s, skew;
                Math::quat quaternion;
                Math::vec4 perspective;
                Math::decompose(wt, s, quaternion, camPos, skew, perspective);

                float aspectRatio = (float)width / (float)height;

                Math::float4x4 projection;
                if (c.projectionType == Assets::CameraComponent::ProjectionType::Perspective)
                {
                    projection = Math::perspective(glm::radians(c.perspectiveFieldOfView), aspectRatio, c.perspectiveNear, c.perspectiveFar);
                }
                else
                {
                    float orthoLeft = -c.orthographicSize * aspectRatio * 0.5f;
                    float orthoRight = c.orthographicSize * aspectRatio * 0.5f;
                    float orthoBottom = -c.orthographicSize * 0.5f;
                    float orthoTop = c.orthographicSize * 0.5f;
                    projection = Math::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, c.orthographicNear, c.orthographicFar);
                }
                SetRendererToSceneCameraProp(fd, c);

                HRay::EndScene(ctx.rd, fd, ctx.commandList, { viewMatrix, projection, camPos, c.perspectiveFieldOfView, (uint32_t)width, (uint32_t)height });
            }
            else
            {
                HRay::EndScene(ctx.rd, fd, ctx.commandList, { editorCamera->view.view, editorCamera->view.projection , editorCamera->transform.position, editorCamera->view.fov, (uint32_t)width, (uint32_t)height });
            }

            if (cameraPrevPos != editorCamera->transform.position || cameraPrevRot != editorCamera->transform.rotation)
            {
                cameraPrevPos = editorCamera->transform.position;
                cameraPrevRot = editorCamera->transform.rotation;

                HRay::Clear(fd);

                // stop preview when camera move
                if (previewMode && cameraAnimation.state & Animation::None)
                {
                    if (std::dynamic_pointer_cast<Editor::OrbitCamera>(editorCamera))
                    {
                        orbitCamera->distance = 0.0f;
                        orbitCamera->focalPoint = editorCamera->transform.position;
                    }

                    StopPreview(false);
                }
            }

            if (clearReq)
            {
                Editor::Clear();
                clearReq = false;
            }

            if (mainCameraEntity)
            {
                UpdateEditorCameraAnimation(scene, mainCameraEntity, ts);
            }
        }
    }

    if(ImGui::IsWindowFocused())
    {
        if (ImGui::IsKeyDown(ImGuiKey_LeftAlt) && ImGui::IsKeyPressed(ImGuiKey_1))
            SetOrbitCamera();

        if (ImGui::IsKeyDown(ImGuiKey_LeftAlt) && ImGui::IsKeyPressed(ImGuiKey_2))
            SetFlyCamera();

        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_F))
            FocusCamera();

        if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_C))
            AlignActiveCameraToView(scene);

        if (ImGui::IsKeyPressed(ImGuiKey_Keypad0) && !(cameraAnimation.state & Animation::Animating))
        {
            if (previewMode)
                StopPreview();
            else
                Preview();
        }
    }

    {
        bool isWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
       
        auto& io = ImGui::GetIO();
        auto& style = ImGui::GetStyle();
        float dpiScale = ImGui::GetWindowDpiScale();
        float scale = ImGui::GetIO().FontGlobalScale * dpiScale;

        auto size = ImGui::GetContentRegionAvail();
        uint32_t w = HE::AlignUp(uint32_t(size.x), 2u);
        uint32_t h = HE::AlignUp(uint32_t(size.y), 2u);

        auto viewportMinRegion = ImGui::GetCursorScreenPos();
        auto viewportMaxRegion = ImGui::GetCursorScreenPos() + ImGui::GetContentRegionAvail();
        auto viewportOffset = ImGui::GetWindowPos();

        {
            ImGui::ScopedStyle ss(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("View"))
                {
                    if (ImGui::BeginMenu("Navegation"))
                    {
                        if (ImGui::MenuItem("Fly Camera", "Alt + 2", (bool)std::dynamic_pointer_cast<Editor::FlyCamera>(editorCamera)))
                            SetFlyCamera();

                        if (ImGui::MenuItem("Orbit Camera", "Alt + 1", (bool)std::dynamic_pointer_cast<Editor::OrbitCamera>(editorCamera)))
                            SetOrbitCamera();

                        if (ImGui::MenuItem("Focus Camera", "LeftCtrl + F"))
                            FocusCamera();

                        if (ImGui::MenuItem("Align Active Camera To View", "LeftCtrl + LeftShift + C"))
                            AlignActiveCameraToView(scene);

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenuBar();
            }
        }

        if (width != w || height != h)
        {
            width = w;
            height = h;
            clearReq |= true;
        }
       
        {
            editorCamera->SetViewportSize(width, height);
            editorCamera->OnUpdate(ts);
        }

        ImVec2 pos;
        ImVec2 viewSize;

        if (fd.renderTarget)
        {
            ImGui::Image(fd.renderTarget.Get(), size);
        }

        // transform Gizmo
        if (isWindowFocused)
        {
            viewportBounds[0] = { viewportMinRegion.x , viewportMinRegion.y };
            viewportBounds[1] = { viewportMaxRegion.x , viewportMaxRegion.y };

            if (ctx.selectedEntity && gizmoType != -1)
            {
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(
                    viewportBounds[0].x,
                    viewportBounds[0].y,
                    viewportBounds[1].x - viewportBounds[0].x,
                    viewportBounds[1].y - viewportBounds[0].y
                );

                const Math::mat4& cameraProjection = editorCamera->view.projection;
                Math::float4x4 cameraView = editorCamera->view.view;

                auto& tc = ctx.selectedEntity.GetComponent<Assets::TransformComponent>();
                Math::float4x4 entityWorldSpaceTransform = ctx.selectedEntity.GetWorldSpaceTransformMatrix();

                bool snap = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
                float snapValue = 0.5f;

                if (gizmoType == ImGuizmo::OPERATION::ROTATE)
                    snapValue = 45.0f;

                float snapValues[3] = { snapValue, snapValue, snapValue };

                bool b = ImGuizmo::Manipulate(
                    Math::value_ptr(cameraView),
                    Math::value_ptr(cameraProjection),
                    (ImGuizmo::OPERATION)gizmoType,
                    ImGuizmo::WORLD,
                    Math::value_ptr(entityWorldSpaceTransform),
                    nullptr,
                    snap ? snapValues : nullptr
                );

                if (ImGuizmo::IsUsing())
                {
                    Math::mat4 parentWorldTransform = ctx.selectedEntity.GetParent().GetWorldSpaceTransformMatrix();
                    Math::mat4 entityLocalSpaceTransform = Math::inverse(parentWorldTransform) * entityWorldSpaceTransform;

                    Math::vec3 position, scale, skew;
                    Math::quat quaternion;
                    Math::vec4 perspective;
                    Math::decompose(entityLocalSpaceTransform, scale, quaternion, position, skew, perspective);

                    tc.position = position;
                    tc.rotation = quaternion;
                    tc.scale = scale;

                    clearReq |= true;
                }
            }
        }

        // Toolbar
        {
            ImGui::ScopedFont sf(Editor::FontType::Blod, Editor::FontSize::BodyMedium);

            Editor::BeginChildView("Toolbar", toolbarCorner);

            ImVec2 buttonSize = ImVec2(28, 28) * io.FontGlobalScale + style.FramePadding * 2;

            if (ImGui::SelectableButton(Icon_Arrow, buttonSize, gizmoType == -1))
            {
                if (!ImGuizmo::IsUsing())
                    gizmoType = -1;
            }

            if (ImGui::SelectableButton(Icon_Move, buttonSize, gizmoType == ImGuizmo::OPERATION::TRANSLATE))
            {
                if (!ImGuizmo::IsUsing())
                    gizmoType = ImGuizmo::OPERATION::TRANSLATE;
            }

            if (ImGui::SelectableButton(Icon_Rotate, buttonSize, gizmoType == ImGuizmo::OPERATION::ROTATE))
            {
                if (!ImGuizmo::IsUsing())
                    gizmoType = ImGuizmo::OPERATION::ROTATE;
            }

            if (ImGui::SelectableButton(Icon_Scale, buttonSize, gizmoType == ImGuizmo::OPERATION::SCALE))
            {
                if (!ImGuizmo::IsUsing())
                    gizmoType = ImGuizmo::OPERATION::SCALE;
            }

            if (ImGui::SelectableButton(Icon_Transform, buttonSize, gizmoType == ImGuizmo::OPERATION::UNIVERSAL))
            {
                if (!ImGuizmo::IsUsing())
                    gizmoType = ImGuizmo::OPERATION::UNIVERSAL;
            }

            Editor::EndChildView();
        }

        // Axis Gizmo
        {
            Editor::BeginChildView("Axis Gizmo", axisGizmoCorner);

            float size = 120 * scale;
            auto viewMatrix = Math::value_ptr(editorCamera->view.view);
            auto projMat = Math::value_ptr(Math::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f));

            if (ImGuizmo::ViewAxisWidget(viewMatrix, projMat, ImGui::GetCursorScreenPos(), size, 0.1f))
            {
                auto transform = Math::inverse(editorCamera->view.view);

                Math::vec3 p, s, skew;
                Math::quat quaternion;
                Math::vec4 perspective;

                Math::decompose(transform, s, quaternion, p, skew, perspective);

                editorCamera->transform.position = p;
                editorCamera->transform.rotation = quaternion;
            }

            ImGui::Dummy({ size, size });
            Editor::EndChildView();
        }

        // Stats
        {
            Editor::BeginChildView("Stats", statsCorner);

            const auto& appStats = HE::Application::GetStats();
            if (ctx.sceneMode == SceneMode::Editor)
                ImGui::Text("CPUMain %.2f ms | FPS %i | Sampels %i | Time %.2f s", appStats.CPUMainTime, appStats.FPS, fd.frameIndex, fd.time);
            else
                ImGui::Text("CPUMain %.2f ms | FPS %i | Time %.2f s | Frame %i / %i | Sample %i / %i", appStats.CPUMainTime, appStats.FPS, fd.time, ctx.frameIndex, ctx.frameEnd, ctx.sampleCount, ctx.maxSamples);

            Editor::EndChildView();
        }
    }
}

void Editor::ViewPortWindow::UpdateEditorCameraAnimation(Assets::Scene* scene, Assets::Entity mainCameraEntity, float ts)
{
    HE_ASSERT(scene);
    HE_ASSERT(mainCameraEntity);

    auto& ctx = Editor::GetContext();

    if (cameraAnimation.state & Editor::Animation::Animating)
    {
        auto wt = mainCameraEntity.GetWorldSpaceTransformMatrix();
        Math::vec3 position, scale, skew;
        Math::quat quaternion;
        Math::vec4 perspective;
        Math::decompose(wt, scale, quaternion, position, skew, perspective);

        cameraAnimation.t += (ts / cameraAnimation.duration) * (cameraAnimation.state & Animation::Forward ? 1 : -1);
        cameraAnimation.t = std::clamp(cameraAnimation.t, 0.0f, 1.0f);
        editorCamera->transform.position = Math::lerp(cameraAnimation.startPosition, position, cameraAnimation.t);
        editorCamera->transform.rotation = Math::slerp(cameraAnimation.startRotation, quaternion, cameraAnimation.t);

        cameraPrevPos = editorCamera->transform.position;
        cameraPrevRot = editorCamera->transform.rotation;

        HRay::Clear(fd);

        // foword
        if (cameraAnimation.t == 1.0f)
        {
            cameraAnimation.state = Animation::None;
        }

        // back
        if (cameraAnimation.t == 0.0f)
        {
            cameraAnimation.state = Animation::None;

            if (std::dynamic_pointer_cast<Editor::OrbitCamera>(editorCamera))
                orbitCamera->overrideTransform = false;
        }
    }
}

void Editor::ViewPortWindow::Preview()
{
    Assets::Scene* scene = Editor::GetScene();
    if (!scene)
        return;

    Assets::Entity entity = Editor::GetSceneCamera(scene);
    if (!entity)
        return;

    previewMode = true;

    // for camera animation
    cameraAnimation.state = Animation::Forward;
    cameraAnimation.startPosition = editorCamera->transform.position;
    cameraAnimation.startRotation = editorCamera->transform.rotation;
    if (std::dynamic_pointer_cast<Editor::OrbitCamera>(editorCamera))
        orbitCamera->overrideTransform = true;
}

void Editor::ViewPortWindow::StopPreview(bool moveBack)
{
    previewMode = false;
    SetRendererToEditorCameraProp(fd);

    // for camera animation
    cameraAnimation.state = moveBack ? Animation::Back : Animation::None;
    if (!moveBack)
    {
        cameraAnimation.t = 0;
        if (std::dynamic_pointer_cast<Editor::OrbitCamera>(editorCamera))
            orbitCamera->overrideTransform = false;
    }
}

void Editor::ViewPortWindow::SetFlyCamera()
{
    HE_PROFILE_FUNCTION();

    flyCamera->transform = orbitCamera->transform;

    editorCamera = flyCamera;
    editorCamera->UpdateView();
    editorCamera->SetViewportSize(width, height);

    Editor::Serialize();
}

void Editor::ViewPortWindow::SetOrbitCamera()
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    orbitCamera->transform = flyCamera->transform;

    Math::vec3 focalPoint = flyCamera->transform.position - flyCamera->transform.GetForward() * 10.0f;
    orbitCamera->focalPoint = focalPoint;
    orbitCamera->distance = 10;

    editorCamera = orbitCamera;
    editorCamera->UpdateView();
    editorCamera->SetViewportSize(width, height);

    Editor::Serialize();
}

void Editor::ViewPortWindow::FocusCamera()
{
    auto selectedEntity = GetSelectedEntity();

    if (selectedEntity)
    {
        auto wt = selectedEntity.GetWorldSpaceTransformMatrix();
        Math::vec3 p, s, skew;
        Math::quat quaternion;
        Math::vec4 perspective;
        Math::decompose(wt, s, quaternion, p, skew, perspective);

        auto r = 2.0f;
        if (selectedEntity.HasComponent<Assets::MeshComponent>())
        {
            auto& mc = selectedEntity.GetComponent<Assets::MeshComponent>();
            auto meshSource = Editor::GetAssetManager().GetAsset<Assets::MeshSource>(mc.meshSourceHandle);
            auto& mesh = meshSource->meshes[mc.meshIndex];
            auto aabb = Math::ConvertBoxToWorldSpace(wt, mesh.aabb);
            r = Math::length(aabb.diagonal());
        }

        editorCamera->Focus(p, r);
    }
}

void Editor::ViewPortWindow::AlignActiveCameraToView(Assets::Scene* scene)
{
    HE_ASSERT(scene);

    auto primaryCamera = Editor::GetSceneCamera(scene);
    if (primaryCamera)
    {
        auto newMatrix = editorCamera->transform.ToMat();
        primaryCamera.SetWorldTransform(newMatrix);
    }
}

void Editor::ViewPortWindow::Serialize(std::ostringstream& out)
{
    out << "\t\t\"gizmoType\" : " << gizmoType << ",\n";

    out << "\t\t\"cameras\" : {\n";
    {
        out << "\t\t\t\"position\" : " << editorCamera->transform.position << ",\n";
        out << "\t\t\t\"rotation\" : " << Math::eulerAngles(editorCamera->transform.rotation) << ",\n";
        out << "\t\t\t\"cameraType\" : " << (std::dynamic_pointer_cast<Editor::OrbitCamera>(editorCamera) ? 0 : 1) << ",\n";

        out << "\t\t\t\"orbitCamera\" : {\n";
        {
            out << "\t\t\t\t\"focalPoint\" : " << orbitCamera->focalPoint << ",\n";
            out << "\t\t\t\t\"distance\" : " << orbitCamera->distance << "\n";
        }
        out << "\t\t\t},\n";

        out << "\t\t\t\"flyCamera\" : {\n";
        {
            out << "\t\t\t\t\"speed\" : " << flyCamera->speed << "\n";
        }
        out << "\t\t\t}\n";
    }
    out << "\t\t},\n";

    out << "\t\t\"toolsCorner\" : {\n";
    {
        out << "\t\t\t\"toolbarCorner\" : \"" << magic_enum::enum_name<Corner>(toolbarCorner) << "\",\n";
        out << "\t\t\t\"statsCorner\" : \"" << magic_enum::enum_name<Corner>(statsCorner) << "\",\n";
        out << "\t\t\t\"axisGizmoCorner\" : \"" << magic_enum::enum_name<Corner>(axisGizmoCorner) << "\"\n";
    }
    out << "\t\t}\n";
}

void Editor::ViewPortWindow::Deserialize(simdjson::dom::element element)
{
    {
        if (!element["gizmoType"].error())
            gizmoType = (int)element["gizmoType"].get_int64().value();
    }

    auto cameras = element["cameras"];
    if (!cameras.error())
    {
        auto positionData = cameras["position"];
        if (!positionData.error())
        {
            Math::float3 position = {
                (float)positionData.get_array().at(0).get_double().value(),
                (float)positionData.get_array().at(1).get_double().value(),
                (float)positionData.get_array().at(2).get_double().value()
            };
    
            orbitCamera->transform.position = position;
            flyCamera->transform.position = position;
        }
    
        auto rotationData = cameras["rotation"];
        if (!rotationData.error())
        {
            Math::quat rotation = Math::quat({
                (float)rotationData.get_array().at(0).get_double().value(),
                (float)rotationData.get_array().at(1).get_double().value(),
                (float)rotationData.get_array().at(2).get_double().value()
            });
    
            orbitCamera->transform.rotation = rotation;
            flyCamera->transform.rotation = rotation;
        }
    
        auto cameraTypeData = cameras["cameraType"];
        if (!cameraTypeData.error())
        {
            uint64_t type = cameraTypeData.get_uint64().value();
            if (type == 0)
                editorCamera = orbitCamera;
            else
                editorCamera = flyCamera;
        }
    
        auto orbitCameraData = cameras["orbitCamera"];
        if (!orbitCameraData.error())
        {
            if (!orbitCameraData["focalPoint"].error())
            {
                orbitCamera->focalPoint = {
                    (float)orbitCameraData["focalPoint"].get_array().at(0).get_double().value(),
                    (float)orbitCameraData["focalPoint"].get_array().at(1).get_double().value(),
                    (float)orbitCameraData["focalPoint"].get_array().at(2).get_double().value()
                };
            }
    
            if (!orbitCameraData["distance"].error())
                orbitCamera->distance = (float)orbitCameraData["distance"].get_double().value();
        }
    
        auto flyCameraData = cameras["flyCamera"];
        if (!flyCameraData.error())
        {
            if (!flyCameraData["speed"].error())
                flyCamera->speed = (float)flyCameraData["speed"].get_double().value();
        }
    }

    auto toolsCorner = element["toolsCorner"];
    if (!toolsCorner.error())
    {
        if (!toolsCorner["toolbarCorner"].error())
        {
            auto str = toolsCorner["toolbarCorner"].get_c_str().value();
            toolbarCorner = magic_enum::enum_cast<Corner>(str).value();
        }

        if (!toolsCorner["axisGizmoCorner"].error())
        {
            auto str = toolsCorner["axisGizmoCorner"].get_c_str().value();
            axisGizmoCorner = magic_enum::enum_cast<Corner>(str).value();
        }

        if (!toolsCorner["statsCorner"].error())
        {
            auto str = toolsCorner["statsCorner"].get_c_str().value();
            statsCorner = magic_enum::enum_cast<Corner>(str).value();
        }
    }
}


#pragma endregion
#pragma region OutputWindow

void Editor::OutputWindow::OnUpdate(HE::Timestep ts)
{
    auto& ctx = GetContext();
    Assets::Scene* scene = ctx.assetManager.GetAsset<Assets::Scene>(ctx.sceneHandle);

    {
        bool isWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

        auto& io = ImGui::GetIO();
        auto& style = ImGui::GetStyle();
        float dpiScale = ImGui::GetWindowDpiScale();
        float scale = ImGui::GetIO().FontGlobalScale * dpiScale;

        auto size = ImGui::GetContentRegionAvail();
        uint32_t w = HE::AlignUp(uint32_t(size.x), 2u);
        uint32_t h = HE::AlignUp(uint32_t(size.y), 2u);

        auto viewportMinRegion = ImGui::GetCursorScreenPos();
        auto viewportMaxRegion = ImGui::GetCursorScreenPos() + ImGui::GetContentRegionAvail();
        auto viewportOffset = ImGui::GetWindowPos();

        if (ctx.fd.renderTarget)
        {
            ImVec2 availableSize = { (float)w, (float)h };
            float imageWidth = (float)ctx.fd.renderTarget->getDesc().width;
            float imageHeight = (float)ctx.fd.renderTarget->getDesc().height;
            float imageAspect = imageWidth / imageHeight;

            ImVec2 drawSize = availableSize;
            float availAspect = availableSize.x / availableSize.y;
            if (availAspect > imageAspect)
                drawSize.x = availableSize.y * imageAspect;
            else
                drawSize.y = availableSize.x / imageAspect;

            ImVec2 cursorPos = ImGui::GetCursorPos();
            ImVec2 offset = (availableSize - drawSize) * 0.5f;
            ImGui::SetCursorPos(cursorPos + offset);

            ImGui::Image(ctx.fd.renderTarget.Get(), drawSize);
        }

        // Stats
        {
            Editor::BeginChildView("Stats", statsCorner);

            const auto& appStats = HE::Application::GetStats();
            if (ctx.sceneMode == SceneMode::Editor)
                ImGui::Text("CPUMain %.2f ms | FPS %i | Sampels %i | Time %.2f s", appStats.CPUMainTime, appStats.FPS, ctx.fd.frameIndex, ctx.fd.time);
            else
                ImGui::Text("CPUMain %.2f ms | FPS %i | Time %.2f s | Frame %i / %i | Sample %i / %i", appStats.CPUMainTime, appStats.FPS, ctx.fd.time, ctx.frameIndex, ctx.frameEnd, ctx.sampleCount, ctx.maxSamples);

            Editor::EndChildView();
        }
    }
}

#pragma endregion
#pragma region HierarchyWindow


void Editor::HierarchyWindow::OnUpdate(HE::Timestep ts)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::ScopedStyle is(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

    {
        ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::BeginChild("Searsh", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##Searsh", Icon_Search"  Searsh", filter.InputBuf, sizeof(filter.InputBuf));
        filter.Build();
        ImGui::EndChild();
    }

    {
        ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(1, 1));
        ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
        ImGui::BeginChild("Entities", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        Assets::Scene* scene = ctx.assetManager.GetAsset<Assets::Scene>(ctx.sceneHandle);
        if (scene)
        {
            auto root = scene->GetRootEntity();
            if (root)
            {
                DrawHierarchy(root, scene);
                ContextWindow(root);
            }
        }
        ImGui::EndChild();
    }
}

void Editor::HierarchyWindow::DrawHierarchy(Assets::Entity parent, Assets::Scene* scene)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    auto& children = parent.GetChildren();
    for (uint32_t i = 0; i < children.size(); i++)
    {
        auto childID = children[i];

        Assets::Entity childEntity = scene->FindEntity(childID);

        auto& nc = childEntity.GetComponent<Assets::NameComponent>();

        bool isSelected = ctx.selectedEntity == childEntity;

        ImGui::PushID((int)childID);

        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow;
        node_flags = childEntity.GetChildren().empty() ? node_flags | ImGuiTreeNodeFlags_Leaf : node_flags;
        node_flags = isSelected ? node_flags | ImGuiTreeNodeFlags_Selected : node_flags;

        bool open = ImGui::TreeNodeEx(nc.name.c_str(), node_flags, "%s  %s", Icon_DICE, nc.name.c_str());
        if (ImGui::IsItemClicked())
            ctx.selectedEntity = childEntity;

        if (childEntity && ImGui::IsItemFocused() && ImGui::IsKeyDown(ImGuiKey::ImGuiKey_Delete))
        {
            Assets::Scene* scene = ctx.assetManager.GetAsset<Assets::Scene>(ctx.sceneHandle);
            if (scene) scene->DestroyEntity(ctx.selectedEntity);
            Editor::Clear();
        }

        if (open)
        {
            if (childEntity)
                DrawHierarchy(childEntity, scene);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

void Editor::HierarchyWindow::ContextWindow(Assets::Entity parent)
{
    ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

    ImGuiPopupFlags flags = ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight;
    if (ImGui::BeginPopupContextWindow("AddEntityMenu", flags))
    {
        AddNewMenu(parent);
        ImGui::EndPopup();
    }
}

void Editor::HierarchyWindow::AddNewMenu(Assets::Entity parent)
{
    auto& ctx = GetContext();

    for (auto& [path, function] : ctx.createEnityFucntions)
    {
        if (ImGui::AutoMenuItem(path.c_str(), nullptr, false, true, ImAutoMenuItemFlags_None))
        {
            Assets::Scene* scene = ctx.assetManager.GetAsset<Assets::Scene>(ctx.sceneHandle);
            function(scene, parent.GetUUID());
            Editor::Clear();
        }
    }
}


#pragma endregion
#pragma region InspectorWindow


void Editor::InspectorWindow::OnUpdate(HE::Timestep ts)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    auto& io = ImGui::GetIO();
    auto& style = ImGui::GetStyle();
    float dpiScale = ImGui::GetWindowDpiScale();
    float scale = ImGui::GetIO().FontGlobalScale * dpiScale;

    ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::ScopedStyle is(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

    if (ctx.selectedEntity)
    {
        auto cf = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

        //ImGui::ScopedColorStack sc(ImGuiCol_Header, ImVec4(0, 0, 0, 0), ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0), ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));

        {
            ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
            ImGui::BeginChild("Name Component", { 0,0 }, cf);

            auto& nc = ctx.selectedEntity.GetComponent<Assets::NameComponent>();
            const float c_FLT_MIN = 1.175494351e-38F;
            ImGui::SetNextItemWidth(-c_FLT_MIN);
            ImGui::InputText("##Name", &nc.name, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::EndChild();
        }

        ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

        {
            if (ImField::BeginBlock("Transform", Icon_Arros, ctx.colors[Color::Transform]))
            {
                if (ImGui::BeginTable("Transform", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    auto& tc = ctx.selectedEntity.GetComponent<Assets::TransformComponent>();

                    ImFieldDrageScalerEvent p = ImField::DragColoredFloat3("Position", &tc.position.x, 0.01f);
                    switch (p)
                    {
                    case ImFieldDrageScalerEvent_None:                                       break;
                    case ImFieldDrageScalerEvent_Edited:                    Editor::Clear(); break;
                    case ImFieldDrageScalerEvent_ResetX: tc.position.x = 0; Editor::Clear(); break;
                    case ImFieldDrageScalerEvent_ResetY: tc.position.y = 0; Editor::Clear(); break;
                    case ImFieldDrageScalerEvent_ResetZ: tc.position.z = 0; Editor::Clear(); break;
                    }

                    Math::float3 degrees = Math::degrees(tc.rotation.GetEuler());
                    ImFieldDrageScalerEvent r = ImField::DragColoredFloat3("Rotation", &degrees.x, 1.0f);
                    switch (r)
                    {
                    case ImFieldDrageScalerEvent_None:                                   break;
                    case ImFieldDrageScalerEvent_Edited:                Editor::Clear(); break;
                    case ImFieldDrageScalerEvent_ResetX: degrees.x = 0; Editor::Clear(); break;
                    case ImFieldDrageScalerEvent_ResetY: degrees.y = 0; Editor::Clear(); break;
                    case ImFieldDrageScalerEvent_ResetZ: degrees.z = 0; Editor::Clear(); break;
                    }
                    tc.rotation = Math::radians(degrees);

                    ImFieldDrageScalerEvent s = ImField::DragColoredFloat3("Scale", &tc.scale.x, 0.01f);
                    switch (s)
                    {
                    case ImFieldDrageScalerEvent_None:                                    break;
                    case ImFieldDrageScalerEvent_Edited:                 Editor::Clear(); break;
                    case ImFieldDrageScalerEvent_ResetX: tc.scale.x = 0; Editor::Clear(); break;
                    case ImFieldDrageScalerEvent_ResetY: tc.scale.y = 0; Editor::Clear(); break;
                    case ImFieldDrageScalerEvent_ResetZ: tc.scale.z = 0; Editor::Clear(); break;
                    }

                    ImGui::EndTable();
                }
            }
            ImField::EndBlock();
        }

        if (ctx.selectedEntity.HasComponent<Assets::CameraComponent>())
        {
            if (ImField::BeginBlock("Camera", Icon_Camera, ctx.colors[Color::Camera]))
            {
                auto& c = ctx.selectedEntity.GetComponent<Assets::CameraComponent>();

                if (ImGui::BeginTable("Camera", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    {
                        int selected = 0;
                        auto currentTypeStr = magic_enum::enum_name<Assets::CameraComponent::ProjectionType>(c.projectionType);
                        auto types = magic_enum::enum_names<Assets::CameraComponent::ProjectionType>();
                        if (ImField::Combo("Projection Type", types, currentTypeStr, selected))
                        {
                            c.projectionType = magic_enum::enum_cast<Assets::CameraComponent::ProjectionType>(types[selected]).value();
                            auto s = magic_enum::enum_name<Assets::CameraComponent::ProjectionType>(c.projectionType);

                            HE_TRACE("c.projectionType {}", s);
                            Editor::Clear();
                        }
                    }

                    if (ImField::Checkbox("Primary", &c.isPrimary)) Editor::Clear();

                    if (c.projectionType == Assets::CameraComponent::ProjectionType::Perspective)
                    {
                        if (ImField::DragFloat("Field Of View", &c.perspectiveFieldOfView)) Editor::Clear();
                        if (ImField::DragFloat("Near", &c.perspectiveNear)) Editor::Clear();
                        if (ImField::DragFloat("Far", &c.perspectiveFar)) Editor::Clear();
                    }

                    //if (c.projectionType == Assets::CameraComponent::ProjectionType::Orthographic)
                    //{
                    //    if (ImField::DragFloat("Size", &c.orthographicSize)) Editor::Clear();
                    //    if (ImField::DragFloat("Near", &c.orthographicNear)) Editor::Clear();
                    //    if (ImField::DragFloat("Far", &c.orthographicFar)) Editor::Clear();
                    //}

                    ImField::SeparatorText("Depth Of Field");
                    if (ImField::Checkbox("Enabled", &c.depthOfField.enabled)) Editor::Clear();
                    if (ImField::Checkbox("Enable Visual Focus Dist", &c.depthOfField.enableVisualFocusDistance)) Editor::Clear();
                    if (ImField::DragFloat("Aperture Radius", &c.depthOfField.apertureRadius, 0.1f, 0.0f)) Editor::Clear();
                    if (ImField::DragFloat("Focus Falloff", &c.depthOfField.focusFalloff, 0.1f, 0.0f)) Editor::Clear();
                    if (ImField::DragFloat("Focus Distance", &c.depthOfField.focusDistance, 0.1f, 0.0f))  Editor::Clear();

                    ImGui::EndTable();
                }
            }
            ImField::EndBlock();
        }

        if (ctx.selectedEntity.HasComponent<Assets::MeshComponent>())
        {
            if (ImField::BeginBlock("Mesh", Icon_Mesh, ctx.colors[Color::Mesh]))
            {
                auto& dm = ctx.selectedEntity.GetComponent<Assets::MeshComponent>();
                auto ms = ctx.assetManager.GetAsset<Assets::MeshSource>(dm.meshSourceHandle);
                auto& meta = ctx.assetManager.GetMetadata(dm.meshSourceHandle);

                if (ImGui::BeginTable("Mesh Component", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    if (ms)
                    {
                        if (dm.meshIndex < ms->meshes.size())
                        {
                            auto& mesh = ms->meshes[dm.meshIndex];

                            if (ImField::Button("Mesh Source", meta.filePath.string().c_str(), { -1, 0 })) Editor::Clear();
                            if (ImField::Button("Mesh", mesh.name.c_str(), { -1, 0 })) Editor::Clear();
                            if (ImField::InputUInt("Index", &dm.meshIndex))
                            {
                                dm.meshIndex = dm.meshIndex >= (uint32_t)ms->meshes.size() ? (uint32_t)ms->meshes.size() - 1 : dm.meshIndex;
                                Editor::Clear();
                            }
                        }
                    }
                    else
                    {
                        auto& meta = ctx.assetManager.GetMetadata(dm.meshSourceHandle);

                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                        if (ImField::Button("Mesh Source", meta.filePath.string().c_str(), { -1, 0 })) Editor::Clear();
                        ImGui::PopStyleColor();
                        if (ImField::InputUInt("Index", &dm.meshIndex)) Editor::Clear();
                    }

                    ImGui::EndTable();
                }
            }
            ImField::EndBlock();


            if (ImField::BeginBlock("Material", Icon_Palette, ctx.colors[Color::Material]))
            {
                auto& dm = ctx.selectedEntity.GetComponent<Assets::MeshComponent>();
                auto ms = ctx.assetManager.GetAsset<Assets::MeshSource>(dm.meshSourceHandle);
                if (ms)
                {
                    auto& mesh = ms->meshes[dm.meshIndex];

                    int i = 0;
                    for (auto& geo : mesh.GetGeometrySpan())
                    {
                        ImGui::ScopedID sid(i++);

                        ImGui::Indent();
                        auto mat = ctx.assetManager.GetAsset<Assets::Material>(geo.materailHandle);

                        if (mat && ImGui::TreeNode(mat->name.c_str()))
                        {
                            ImGui::TreePop();

                            ImVec2 size = ImVec2(60) * scale;

                            if (ImGui::BeginTable("Materials", 2, ImGuiTableFlags_SizingFixedFit))
                            {
                                ImGui::Indent();

                                ImField::Separator();
                                auto baseT = ctx.assetManager.GetAsset<Assets::Texture>(mat->baseTextureHandle);
                                if (ImField::ImageButton("Bace Texture", baseT ? baseT->texture.Get() : ctx.board.Get(), size)) Editor::Clear();

                                TextureHandler(mat, mat->baseTextureHandle);
                                if (ImField::ColorEdit4("Bace Color", &mat->baseColor.x)) Editor::Clear();

                                ImField::Separator();
                                auto normalT = ctx.assetManager.GetAsset<Assets::Texture>(mat->normalTextureHandle);
                                if (ImField::ImageButton("Normal Texture", normalT ? normalT->texture.Get() : ctx.board.Get(), size)) Editor::Clear();
                                TextureHandler(mat, mat->normalTextureHandle);

                                ImField::Separator();
                                auto metallicRoughnessT = ctx.assetManager.GetAsset<Assets::Texture>(mat->metallicRoughnessTextureHandle);
                                if (ImField::ImageButton("Metallic Roughness Texture", metallicRoughnessT ? metallicRoughnessT->texture.Get() : ctx.board.Get(), size)) Editor::Clear();
                                TextureHandler(mat, mat->metallicRoughnessTextureHandle);
                                if (ImField::DragFloat("Metallic", &mat->metallic, 0.01f, 0.0f, 1.0f)) Editor::Clear();
                                if (ImField::DragFloat("Roughness", &mat->roughness, 0.01f, 0.0f, 1.0f)) Editor::Clear();

                                ImField::Separator();
                                auto emissiveT = ctx.assetManager.GetAsset<Assets::Texture>(mat->emissiveTextureHandle);
                                if (ImField::ImageButton("Emissive Texture", emissiveT ? emissiveT->texture.Get() : ctx.board.Get(), size)) Editor::Clear();
                                TextureHandler(mat, mat->emissiveTextureHandle);
                                if (ImField::ColorEdit3("Emissive Color", &mat->emissiveColor.x)) Editor::Clear();
                                if (ImField::DragFloat("Emissive Exposure", &mat->emissiveEV, 0.01f)) Editor::Clear();

                                ImField::Separator();
                                if (ImField::DragFloat2("Offset", &mat->offset.x, 0.01f)) Editor::Clear();
                                if (ImField::DragFloat2("Scale", &mat->scale.x, 0.01f)) Editor::Clear();
                                float deg = Math::degrees(mat->rotation);
                                if (ImField::DragFloat("Rotation", &deg, 0.1f)) Editor::Clear();
                                mat->rotation = Math::radians(deg);

                                int selected = 0;
                                auto currentTypeStr = magic_enum::enum_name<Assets::UVSet>(mat->uvSet);
                                auto types = magic_enum::enum_names<Assets::UVSet>();
                                if (ImField::Combo("UV", types, currentTypeStr, selected))
                                {
                                    mat->uvSet = magic_enum::enum_cast<Assets::UVSet>(types[selected]).value();
                                    Editor::Clear();
                                }

                                ImGui::Unindent();

                                ImGui::EndTable();
                            }
                        }
                        ImGui::Unindent();
                    }
                }
            }
            ImField::EndBlock();
        }

        if (ctx.selectedEntity.HasComponent<Assets::DirectionalLightComponent>())
        {
            if (ImField::BeginBlock("Directional Light", Icon_Sun, ctx.colors[Color::Light]))
            {
                auto& c = ctx.selectedEntity.GetComponent<Assets::DirectionalLightComponent>();

                if (ImGui::BeginTable("Directional Light", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    if (ImField::ColorEdit3("Color", &c.color.x)) Editor::Clear();
                    if (ImField::DragFloat("Intensity", &c.intensity)) Editor::Clear();
                    if (ImField::DragFloat("AngularRadius", &c.angularRadius)) Editor::Clear();
                    if (ImField::DragFloat("HaloSize", &c.haloSize)) Editor::Clear();
                    if (ImField::DragFloat("HaloFalloff", &c.haloFalloff)) Editor::Clear();

                    ImGui::EndTable();
                }
            }
            ImField::EndBlock();
        }

        if (ctx.selectedEntity.HasComponent<Assets::DynamicSkyLightComponent>())
        {
            if (ImField::BeginBlock("Dynamic Sky Light", Icon_Sun, ctx.colors[Color::Light]))
            {
                auto& c = ctx.selectedEntity.GetComponent<Assets::DynamicSkyLightComponent>();

                if (ImGui::BeginTable("Dynamic Sky Light", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    if (ImField::ColorEdit3("Ground Color", &c.groundColor.x)) Editor::Clear();
                    if (ImField::ColorEdit3("Horizon Sky Color", &c.horizonSkyColor.x)) Editor::Clear();
                    if (ImField::ColorEdit3("Zenith Sky Color", &c.zenithSkyColor.x)) Editor::Clear();

                    ImGui::EndTable();
                }
            }
            ImField::EndBlock();
        }
    }

    if (ctx.selectedEntity)
    {
        ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

        float x = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy({ x, 10 });

        ImGui::ShiftCursorX(x * 0.2f);
        if (ImGui::Button(Icon_Plus"  Add Component", { x * 0.6f, 0 }))
        {
            ImGui::OpenPopup("AddComponent");
        }

        if (ImGui::BeginPopup("AddComponent"))
        {
            DisplayAddComponentEntry<Assets::TransformComponent>(Icon_Transform"  TransForm");
            DisplayAddComponentEntry<Assets::MeshComponent>(Icon_Mesh"  Mesh");
            DisplayAddComponentEntry<Assets::DirectionalLightComponent>(Icon_Sun"  Directional Light");
            DisplayAddComponentEntry<Assets::DynamicSkyLightComponent>(Icon_Sun"  Dynamic Sky Light");
            DisplayAddComponentEntry<Assets::CameraComponent>(Icon_Camera"  Camera");

            ImGui::EndPopup();
        }
    }
}

bool Editor::InspectorWindow::TextureHandler(Assets::Material* mat, Assets::AssetHandle handle)
{
    auto& ctx = GetContext();

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
        {
            Assets::AssetHandle handle = *(Assets::AssetHandle*)payload->Data;
            if (ctx.assetManager.GetAssetType(handle) == Assets::AssetType::Texture2D)
            {

                return true;
            }
            else
            {
                HE_CORE_WARN("Wrong asset type!");
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Remove Texture
    if (ctx.assetManager.IsAssetHandleValid(handle) && ImGui::IsItemFocused() && ImGui::IsKeyDown(ImGuiKey::ImGuiKey_Delete))
    {
        if (mat->baseTextureHandle == handle)
            mat->baseTextureHandle = 0;
        else if (mat->normalTextureHandle == handle)
            mat->normalTextureHandle = 0;
        else if (mat->metallicRoughnessTextureHandle == handle)
            mat->metallicRoughnessTextureHandle = 0;
        else if (mat->emissiveTextureHandle == handle)
            mat->emissiveTextureHandle = 0;

        return true;
    }

    return false;
}

template<typename T>
void Editor::InspectorWindow::DisplayAddComponentEntry(const std::string& entryName)
{
    auto& ctx = GetContext();

    if (ctx.selectedEntity && !ctx.selectedEntity.HasComponent<T>())
    {
        if (ImGui::MenuItem(entryName.c_str()))
        {
            ctx.selectedEntity.AddComponent<T>();

            ImGui::CloseCurrentPopup();
        }
    }
}


#pragma endregion
#pragma region AssetManagerWindow


void  Editor::AssetManagerWindow::OnUpdate(HE::Timestep ts)
{
    HE_PROFILE_FUNCTION();

    auto& am = Editor::GetContext().assetManager;

    ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::BeginChild("Searsh", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_ResizeY | ImGuiChildFlags_AutoResizeY);

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##Searsh", Icon_Search"  Searsh", filter.InputBuf, sizeof(filter.InputBuf));
    filter.Build();
    ImGui::EndChild();

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("taps", tab_bar_flags))
    {
        if (ImGui::BeginTabItem("Assets"))
        {
            ImGui::BeginChild("item", ImVec2(ImGui::GetContentRegionAvail().x, -1), ImGuiChildFlags_AlwaysUseWindowPadding);

            if (filter.IsActive())
            {
                for (auto& [handle, metaData] : am.metaMap)
                {
                    std::string pathStr = metaData.filePath.string();
                    std::string handleStr = std::to_string(handle);
                    if (!filter.PassFilter(handleStr.c_str()) && !filter.PassFilter(pathStr.c_str()) && !filter.PassFilter(magic_enum::enum_name<Assets::AssetType>(metaData.type).data()))
                        continue;

                    DrawAssetItem(am, handle, metaData);
                }
            }
            else
            {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(am.metaMap.size()));
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        auto& [handle, metaData] = *(std::next(am.metaMap.begin(), i));

                        DrawAssetItem(am, handle, metaData);
                    }
                }
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Manager"))
        {
            auto flags = ImGuiTreeNodeFlags_DefaultOpen;

            if (ImField::BeginBlock("Manager"))
            {
                if (ImGui::BeginTable("Manager", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    auto currentTypeStr = magic_enum::enum_name<Assets::AssetImportingMode>(am.desc.importMode);
                    auto types = magic_enum::enum_names<Assets::AssetImportingMode>();
                    int selected = 0;
                    if (ImField::Combo("Import Mode", types, currentTypeStr, selected))
                    {
                        auto t = magic_enum::enum_cast<Assets::AssetImportingMode>(types[selected]).value();
                        am.desc.importMode = t;
                    }

                    ImField::TextLinkOpenURL("Asset Registry", am.desc.assetsRegistryFilePath.string().c_str());
                    ImField::Text("Registered Assets", "%zd", am.metaMap.size());
                    ImField::Text("Subscribers", "%zd", am.subscribers.size());
                    ImField::Text("Async Task Count", "%zd", am.asyncTaskCount);

                    ImGui::EndTable();
                }
            }
            ImField::EndBlock();


            if (ImField::BeginBlock("Loaded Assets"))
            {
                if (ImGui::BeginTable("Loaded Assets", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    ImField::Text("Textures", "%zd", am.registry.view<Assets::Texture>().size());
                    ImField::Text("Materials", "%zd", am.registry.view<Assets::Material>().size());
                    ImField::Text("MeshSources", "%zd", am.registry.view<Assets::MeshSource>().size());
                    ImField::Text("Scenes", "%zd", am.registry.view<Assets::Scene>().size());

                    ImGui::EndTable();
                }
            }
            ImField::EndBlock();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void Editor::AssetManagerWindow::DrawAssetItem(Assets::AssetManager& am, Assets::AssetHandle handle, Assets::AssetMetadata& metaData)
{
    HE_PROFILE_FUNCTION();

    ImGui::PushID((int)handle);

    bool loaded = am.IsAssetLoaded(handle);

    if (loaded) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.3f, 1.0f));

    {
        ImVec2 size = ImGui::CalcTextSize("Handle") + ImVec2(20, 0);
        bool fileExists = std::filesystem::exists(am.GetAssetFileSystemPath(handle));
        bool validHandle = am.IsAssetHandleValid(handle);

        ImGui::ScopedID id((uint32_t)handle);

        if (ImGui::BeginTable("AssetsManager", 2, ImGuiTableFlags_SizingFixedFit))
        {
            {
                auto currentTypeStr = magic_enum::enum_name<Assets::AssetType>(metaData.type);
                auto types = magic_enum::enum_names<Assets::AssetType>();

                int selected = 0;
                if (ImField::Combo("Asset Type", types, currentTypeStr, selected))
                {
                    auto t = magic_enum::enum_cast<Assets::AssetType>(types[selected]).value();
                    am.metaMap[handle].type = t;
                    am.Serialize();
                }
            }

            {
                uint64_t h = handle;
                if (!validHandle) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                ImField::InputScalar("Handle", &h, ImGuiDataType_U64);
                if (!validHandle) ImGui::PopStyleColor();
            }

            {
                if (!fileExists) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                std::string str = metaData.filePath.string();
                if (ImField::InputText("Path", &str, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    Assets::AssetMetadata metaData;
                    metaData.filePath = str;
                    metaData.type = metaData.type;
                    am.UpdateMetadate(handle, metaData);
                }
                if (!fileExists) ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }

        {
            float width = ImGui::GetContentRegionAvail().x / 4 - ImGui::GetCurrentContext()->CurrentWindow->WindowPadding.x;

            if (ImGui::Button("Remove", ImVec2(width, 0)))
            {
                am.RemoveAsset(handle);
            }
            ImGui::SameLine();

            ImGui::BeginDisabled(!am.IsAssetLoaded(handle));
            if (ImGui::Button("Unload", ImVec2(width, 0)))
            {
                am.UnloadAsset(handle);
            }

            ImGui::SameLine();

            if (ImGui::Button("Reload", ImVec2(width, 0)) && fileExists)
            {
                am.ReloadAsset(handle);
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("Open", ImVec2(-1, 0)) && fileExists)
            {
                HE::FileSystem::Open(am.GetAssetFileSystemPath(handle));
            }
        }
    }
    if (loaded) ImGui::PopStyleColor();

    ImGui::Dummy({ -1,4 });
    ImGui::Separator();
    ImGui::Dummy({ -1,4 });

    ImGui::PopID();
}


#pragma endregion
#pragma region RendererSettingsWindow


void Editor::RendererSettingsWindow::OnUpdate(HE::Timestep ts)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::ScopedStyle is(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

    auto cf = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

    if (ImField::BeginBlock("Settings"))
    {
        if (ImGui::BeginTable("Settings", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImField::Checkbox("VSync", &HE::Application::GetWindow().swapChain->desc.vsync);
    
            // TODO
            //if (ImField::DragInt("Max Lighte Bounces", &ctx.rd.sceneInfo.settings.maxLighteBounces)) Editor::Clear();
            //if (ImField::DragInt("Max Samples", &ctx.rd.sceneInfo.settings.maxSamples)) Editor::Clear();
            //if (ImField::DragFloat("Gamma", &ctx.rd.sceneInfo.settings.gamma)) Editor::Clear();
    
            ImGui::EndTable();
        }
    }
    ImField::EndBlock();

    if (ImField::BeginBlock("Output"))
    {
        if (ImGui::BeginTable("Output", 2, ImGuiTableFlags_SizingFixedFit))
        {
            if (ImField::DragInt("Width", &ctx.width)) Editor::Clear();
            if (ImField::DragInt("Height", &ctx.height)) Editor::Clear();

            ImField::DragInt("Frame Start", &ctx.frameStart);
            ImField::DragInt("Frame End", &ctx.frameEnd);
            ImField::DragInt("Frame Step", &ctx.frameStep);
            ImField::DragInt("Max Samples", &ctx.maxSamples);

            ImGui::EndTable();
        }

        {
            ImGui::Indent(8);

            {
                ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

                {
                    ImGui::ScopedFont sf(Editor::FontType::Blod);
                    ImGui::TextUnformatted("Output Path");
                }
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50);

                bool validPath = std::filesystem::exists(ctx.outputPath);

                if (!validPath) ImGui::PushStyleColor(ImGuiCol_Text, ctx.colors[Editor::Color::Error]);
                ImGui::InputTextWithHint("##outputPath", "Output Path", &ctx.outputPath);
                if (!validPath) ImGui::PopStyleColor();

                ImGui::SameLine(0, 4);
                if (ImGui::Button(Icon_Folder, { -1, 0 }))
                {
                    auto path = HE::FileDialog::SelectFolder().string();
                    if (!path.empty())
                        ctx.outputPath = path;
                }
            }

            {
                const char* state = ctx.sceneMode == SceneMode::Runtime ? "Cancel" : "Render";
                ImGui::ScopedColorStack sc(
                    ImGuiCol_Button, ctx.sceneMode == SceneMode::Runtime ? ctx.colors[Color::Dangerous] : ImGui::GetStyle().Colors[ImGuiCol_Button],
                    ImGuiCol_ButtonActive, ctx.sceneMode == SceneMode::Runtime ? ctx.colors[Color::DangerousActive] : ImGui::GetStyle().Colors[ImGuiCol_ButtonActive],
                    ImGuiCol_ButtonHovered, ctx.sceneMode == SceneMode::Runtime ? ctx.colors[Color::DangerousHovered] : ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]
                );
            
                if (ImGui::Button(state, { -1 , 0 }))
                {
                    if (ctx.sceneMode == SceneMode::Runtime)
                        Editor::Stop();
                    else
                        Editor::Animate();
                }
            }


            if (ctx.sceneMode == SceneMode::Runtime)
            {
                ImGui::ScopedColor sc(ImGuiCol_PlotHistogram, ctx.colors[Color::Info]);

                float frameProgress = (float)ctx.frameIndex / (float)ctx.frameEnd;
                float sampleProgress = (float)ctx.sampleCount / (float)ctx.maxSamples;
                float combinedProgress = (frameProgress + sampleProgress / ctx.frameEnd);

                std::string overlay = std::format("Scene {:>3.0f}%  |  Frame {:>3.0f}%", combinedProgress * 100.0f, sampleProgress * 100.0f);
                ImGui::ProgressBar(combinedProgress, ImVec2(-1, 0), overlay.c_str());
            }

            ImGui::Unindent(8);
        }
    }
    ImField::EndBlock();

    if (ImField::BeginBlock("Stats"))
    {
        if (ImGui::BeginTable("Scene", 2, ImGuiTableFlags_SizingFixedFit))
        {
            //ImField::Text("Geometry Count", "%zd", ctx.rd.geometryCount);
            //ImField::Text("Instance Count", "%zd", ctx.rd.instanceCount);
            //ImField::Text("Material Count", "%zd", ctx.rd.materialCount);
            //ImField::Text("Texture Count", "%zd", ctx.rd.textureCount);
            //ImField::Text("Directional Light Count", "%zd", ctx.rd.sceneInfo.light.directionalLightCount);

            Assets::Scene* scene = ctx.assetManager.GetAsset<Assets::Scene>(ctx.sceneHandle);
            if (scene)
            {
                auto transformComponentCount = scene->registry.view<Assets::TransformComponent>().size();
                auto cameraComponentCount = scene->registry.view<Assets::CameraComponent>().size();
                auto meshComponentCount = scene->registry.view<Assets::MeshComponent>().size();
                auto directionalLightComponentCount = scene->registry.view<Assets::DirectionalLightComponent>().size();
                auto dynamicSkyLightComponentCount = scene->registry.view<Assets::DynamicSkyLightComponent>().size();

                ImField::Text("Transform Component Count", "%zd", transformComponentCount);
                ImField::Text("Camera Component Count", "%zd", cameraComponentCount);
                ImField::Text("Mesh Component Count", "%zd", meshComponentCount);
                ImField::Text("Directional Light Component Count", "%zd", directionalLightComponentCount);
                ImField::Text("Dynamic Sky Light Component Count", "%zd", dynamicSkyLightComponentCount);
            }

            ImGui::EndTable();
        }
    }
    ImField::EndBlock();
}


#pragma endregion