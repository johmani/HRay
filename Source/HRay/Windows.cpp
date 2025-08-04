#include "HydraEngine/Base.h"
#include "HRay/Icons.h"

#if NVRHI_HAS_D3D12
#include "Embeded/dxil/Compositing_Main.bin.h"
#endif

#if NVRHI_HAS_VULKAN
#include "Embeded/spirv/Compositing_Main.bin.h"
#endif

import HE;
import nvrhi;
import Assets;
import ImGui;
import Editor;
import std;
import Tiny2D;

#pragma region ViewPortWindow

static void CreateOrResizeRenderTarget(Editor::ViewPortWindow* viewPortWindow, nvrhi::Format format, uint32_t width, uint32_t height)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    nvrhi::TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    desc.format = format;
    desc.keepInitialState = true;
    desc.isRenderTarget = false;
    desc.isUAV = true;

    desc.debugName = "viewPortOutput";
    viewPortWindow->compositeTarget = ctx.device->createTexture(desc);

    desc.debugName = "idTarget";
    desc.format = nvrhi::Format::R32_UINT;
    viewPortWindow->idTarget = ctx.device->createTexture(desc);

    viewPortWindow->compositeBindingSet.Reset();
    viewPortWindow->pixelReadbackPass.bindingSet.Reset();
}

void Editor::ViewPortWindow::OnCreate()
{
    Editor::GetWindowDesc(this).menuBar = true;

    orbitCamera = HE::CreateScope<Editor::OrbitCamera>(60.0f, float(width) / float(height), 0.1f, 1000.0f);
    flyCamera = HE::CreateScope<Editor::FlyCamera>(60.0f, float(width) / float(height), 0.1f, 1000.0f);
    editorCamera = orbitCamera;

    auto& ctx = GetContext();

    // Binding Layout
    {
        HE_PROFILE_SCOPE("createBindingLayout");

        nvrhi::BindingLayoutDesc desc;
        desc.visibility = nvrhi::ShaderType::All;
        desc.bindings = {
           nvrhi::BindingLayoutItem::Texture_SRV(0),
           nvrhi::BindingLayoutItem::Texture_SRV(1),
           nvrhi::BindingLayoutItem::Texture_SRV(2),
           nvrhi::BindingLayoutItem::Texture_SRV(3),
           nvrhi::BindingLayoutItem::Texture_SRV(4),
           nvrhi::BindingLayoutItem::Texture_SRV(5),
           nvrhi::BindingLayoutItem::Texture_UAV(0),
           nvrhi::BindingLayoutItem::Texture_UAV(1),
        };

        compositeBindingLayout = ctx.device->createBindingLayout(desc);
        HE_ASSERT(compositeBindingLayout);
    }

    // Shader
    {
        nvrhi::ShaderDesc desc;
        desc.shaderType = nvrhi::ShaderType::Compute;
        desc.entryName = "Main";
        cs = HE::RHI::CreateStaticShader(ctx.device, STATIC_SHADER(Compositing_Main), nullptr, desc);
    }

    // Compute
    {
        computePipeline = ctx.device->createComputePipeline({ cs, { compositeBindingLayout } });
        HE_VERIFY(computePipeline);
    }
}

static void DrawIcon(
    Math::float3 camPosition,
    Math::float3 position,
    bool isSelected,
    const ImVec4& selectionColor,
    nvrhi::ITexture* texture,
    uint32_t id
)
{
    Math::float3 directionToCamera = Math::normalize(camPosition - position);
    Math::quat iconRotationQuat = Math::quatLookAt(-directionToCamera, { 0.0f, 1.0f, 0.0f });
    Math::float3 iconRotationEuler = Math::eulerAngles(iconRotationQuat);

    float dis = Math::length(camPosition - position); // distans from to icon - camera
    float alfa = Math::clamp(float(pow(dis - 0.5f, 2) * dis) / 4.5f, 0.0f, 1.0f); // this make alfa value becoming (0-1) range under 2 meter

    Math::vec4 color = isSelected ? Math::vec4(selectionColor.x, selectionColor.y, selectionColor.z, alfa) : Math::vec4{ 1.0f, 1.0f, 1.0f, alfa };
   
    Tiny2D::DrawQuad({
        .position = position,
        .rotation = iconRotationQuat,
        .scale = { 1.0f, 1.0f, 1.0f },
        .minUV = { 0.0f,0.0f },
        .maxUV = { 1.0f,1.0f },
        .color = color,
        .texture = texture,
        .id = id
    });
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

            Math::float4x4 viewMatrix;
            Math::float4x4 projection;
            Math::float3 camPos;
            float fov;

            if (previewMode && mainCameraEntity && cameraAnimation.state & Animation::None)
            {
                auto& c = mainCameraEntity.GetComponent<Assets::CameraComponent>();
                auto wt = mainCameraEntity.GetWorldSpaceTransformMatrix();
                viewMatrix = Math::inverse(wt);
                fov = c.perspectiveFieldOfView;

                Math::float3 s, skew;
                Math::quat quaternion;
                Math::vec4 perspective;
                Math::decompose(wt, s, quaternion, camPos, skew, perspective);

                float aspectRatio = (float)width / (float)height;

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

                Editor::SetRendererToSceneCameraProp(fd, c);
            }
            else
            {
                viewMatrix = editorCamera->view.view;
                projection = editorCamera->view.projection;
                camPos = editorCamera->transform.position;
                fov = editorCamera->view.fov;
            }

            HRay::BeginScene(ctx.rd, fd);

            if (!pixelReadbackPass.device)
                pixelReadbackPass.Init(ctx.device);

            {
                auto format = HRay::GetColorTarget(fd)->getDesc().format;

                if (!compositeTarget)
                    CreateOrResizeRenderTarget(this, format, width, height);

                Tiny2D::BeginScene(
                    tiny2DView,
                    ctx.commandList,
                    {
                        projection * viewMatrix,
                        { width, height },
                        format,
                        8
                    }
                );
            }
            
            {
                auto view = scene->registry.view<Assets::CameraComponent>();
                for (auto e : view)
                {
                    Assets::Entity entity = { e, scene };
                    auto& camera = entity.GetComponent<Assets::CameraComponent>();

                    auto wt = entity.GetWorldSpaceTransformMatrix();

                    Math::float3 cameraPosition, scale, skew;
                    Math::quat cameraRotation;
                    Math::float4 perspective;
                    Math::decompose(wt, scale, cameraRotation, cameraPosition, skew, perspective);

                    Math::vec4 selectionColor = { 0.9f,0.8f ,0.2f ,1.0f };
                    bool isSelected = entity == Editor::GetSelectedEntity();

                    DrawIcon(
                        editorCamera->transform.position,
                        cameraPosition,
                        isSelected,
                        Editor::GetColor(Editor::Color::ViewPortSelected),
                        Editor::GetIcon(Editor::AppIcons::Camera),
                        (uint32_t)e
                    );

                    if (!isSelected)
                        continue;

                    float fov = Math::radians(camera.perspectiveFieldOfView);
                    float aspectRatio = float(width) / float(height);
                    float nearPlane = camera.perspectiveNear;
                    float farPlane = camera.perspectiveFar * 0.002f;
                    float tanHalfFOV = std::tan(fov / 2.0f);

                    // Step 3: Calculate near plane corners
                    float nearHeight = 2.0f * nearPlane * tanHalfFOV;
                    float nearWidth = nearHeight * aspectRatio;
                    Math::float3 nearCenter = cameraPosition + Math::rotate(cameraRotation, Math::float3(0.0f, 0.0f, -nearPlane));
                    Math::float3 nearTopLeft = nearCenter + Math::rotate(cameraRotation, Math::float3(-nearWidth / 2.0f, nearHeight / 2.0f, 0.0f));
                    Math::float3 nearTopRight = nearCenter + Math::rotate(cameraRotation, Math::float3(nearWidth / 2.0f, nearHeight / 2.0f, 0.0f));
                    Math::float3 nearBottomLeft = nearCenter + Math::rotate(cameraRotation, Math::float3(-nearWidth / 2.0f, -nearHeight / 2.0f, 0.0f));
                    Math::float3 nearBottomRight = nearCenter + Math::rotate(cameraRotation, Math::float3(nearWidth / 2.0f, -nearHeight / 2.0f, 0.0f));

                    // Step 4: Calculate far plane corners
                    float farHeight = 2.0f * farPlane * tanHalfFOV;
                    float farWidth = farHeight * aspectRatio;
                    Math::float3 farCenter = cameraPosition + Math::rotate(cameraRotation, Math::float3(0.0f, 0.0f, -farPlane));
                    Math::float3 farTopLeft = farCenter + Math::rotate(cameraRotation, Math::float3(-farWidth / 2.0f, farHeight / 2.0f, 0.0f));
                    Math::float3 farTopRight = farCenter + Math::rotate(cameraRotation, Math::float3(farWidth / 2.0f, farHeight / 2.0f, 0.0f));
                    Math::float3 farBottomLeft = farCenter + Math::rotate(cameraRotation, Math::float3(-farWidth / 2.0f, -farHeight / 2.0f, 0.0f));
                    Math::float3 farBottomRight = farCenter + Math::rotate(cameraRotation, Math::float3(farWidth / 2.0f, -farHeight / 2.0f, 0.0f));

                    //Math::float3 frustumColor = { 1.0f, 1.0f, 1.0f };
                    float dis = Math::distance(farTopRight, farBottomRight);
                    Math::float3 upPoint = farCenter + Math::normalize(farTopRight - farBottomRight) * dis;

                    std::array<Math::float3, 5> nearPlanePoints = { nearTopLeft, nearTopRight, nearBottomRight, nearBottomLeft, nearTopLeft };
                    std::array<Math::float3, 7> farPlanePoints = { farTopLeft, farTopRight, farBottomRight, farBottomLeft ,farTopLeft, upPoint, farTopRight };
                    std::array<Math::float3, 8> nearFarConnectionPoint = { nearTopLeft, farTopLeft, nearTopRight, farTopRight, nearBottomLeft, farBottomLeft, nearBottomRight, farBottomRight };
                    std::array<Math::float3, 3> upCameraPoints = { };

                    Tiny2D::DrawLineStrip(nearPlanePoints, selectionColor);
                    Tiny2D::DrawLineStrip(farPlanePoints, selectionColor);
                    Tiny2D::DrawLineList(nearFarConnectionPoint, selectionColor);
                }
            }

            {
                auto& assetRegistry = Editor::GetAssetManager().registry;
                auto view = Editor::GetAssetManager().registry.view<Assets::Material>();
                for (auto e : view)
                {
                    Assets::Asset materalAsset = { e, &Editor::GetAssetManager() };
                    HRay::SubmitMaterial(ctx.rd, fd, materalAsset);
                }
            }

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

                        HRay::SubmitMesh(ctx.rd, fd, asset, mesh, wt, (uint32_t)e, ctx.commandList);

                        if (debug.enableMeshAABB)
                        {
                            Math::box3 box = Math::ConvertBoxToWorldSpace(wt, mesh.aabb);
                            Tiny2D::DrawAABB({ 
                                .min = box.min - 0.001f, 
                                .max = box.max + 0.001f, 
                                .color = Math::float4(1.0f,1.0f,1.0f, debug.colorOpacity), 
                                .thickness = 1
                            });
                        }

                        if (debug.enableMeshNormals || debug.enableMeshTangents || debug.enableMeshBitangents)
                        {
                            auto positions = mesh.GetAttributeSpan<Math::float3>(Assets::VertexAttribute::Position);
                            auto normals = mesh.GetAttributeSpan<uint32_t>(Assets::VertexAttribute::Normal);
                            auto tangents = mesh.GetAttributeSpan<uint32_t>(Assets::VertexAttribute::Tangent);

                            if (debug.enableMeshBitangents || debug.enableMeshTangents || debug.enableMeshNormals)
                            {
                                for (uint32_t i = 0; i < positions.size(); i++)
                                {
                                    Math::float4 pos = wt * Math::float4(positions[i], 1.0f);
                                    Math::float3 position = { pos.x, pos.y, pos.z };

                                    Math::float3 normal = Math::snorm8ToVector<3>(normals[i]);
                                    normal = wt * Math::float4(normal, 0.0f);
                                    normal = Math::normalize(normal);

                                    Math::float4 tang = Math::snorm8ToVector<4>(tangents[i]);
                                    Math::float4 tangent = { tang.x, tang.y, tang.z, tang.w };

                                    Math::float3 tangentVec = wt * Math::float4(tangent.x, tangent.y, tangent.z, 0);
                                    Math::float3 bitangentVec = Math::cross(normal, tangentVec) * tangent.w;

                                    tangentVec = Math::normalize(tangentVec);
                                    bitangentVec = Math::normalize(bitangentVec);

                                    if (debug.enableMeshNormals)
                                    {
                                        Tiny2D::DrawLine({
                                            .from = position,
                                            .to = position + normal * debug.lineLength,
                                            .fromColor = Math::float4(1.0f,0.0f, 0.0f, debug.colorOpacity),
                                            .toColor = Math::float4(1.0f,0.0f, 0.0f, debug.colorOpacity),
                                            .thickness = 1.0f
                                        });
                                    }

                                    if (debug.enableMeshTangents)
                                    {
                                        Tiny2D::DrawLine({
                                            .from = position,
                                            .to = position + tangentVec * debug.lineLength,
                                            .fromColor = Math::float4(0.0f,1.0f, 0.0f, debug.colorOpacity),
                                            .toColor = Math::float4(0.0f, 1.0f, 0.0f, debug.colorOpacity),
                                            .thickness = 1.0f
                                        });
                                    }

                                    if (debug.enableMeshBitangents)
                                    {
                                        Tiny2D::DrawLine({
                                            .from = position,
                                            .to = position + bitangentVec * debug.lineLength,
                                            .fromColor = Math::float4(0.0f, 0.0f, 1.0f, debug.colorOpacity),
                                            .toColor = Math::float4(0.0f, 0.0f, 1.0f, debug.colorOpacity),
                                            .thickness = 1.0f
                                        });
                                    }
                                }
                            }
                        }
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

                    Math::float3 position, scale, skew;
                    Math::quat rotation;
                    Math::vec4 perspective;
                    Math::decompose(wt, scale, rotation, position, skew, perspective);

                    DrawIcon(
                        editorCamera->transform.position,
                        position,
                        entity == Editor::GetSelectedEntity(),
                        Editor::GetColor(Editor::Color::ViewPortSelected),
                        Editor::GetIcon(Editor::AppIcons::DirectionalLight),
                        (uint32_t)e
                    );

                    HRay::SubmitDirectionalLight(ctx.rd, fd, light, wt);
                }
            }

            {
                auto view = scene->registry.view<Assets::SkyLightComponent>();
                for (auto e : view)
                {
                    Assets::Entity entity = { e, scene };
                    auto& skyLight = entity.GetComponent<Assets::SkyLightComponent>();

                    auto wt = entity.GetWorldSpaceTransformMatrix();

                    Math::float3 position, scale, skew;
                    Math::quat rotation;
                    Math::vec4 perspective;
                    Math::decompose(wt, scale, rotation, position, skew, perspective);
                    Math::float3 euler = Math::eulerAngles(rotation);

                    DrawIcon(
                        editorCamera->transform.position,
                        position,
                        entity == Editor::GetSelectedEntity(),
                        Editor::GetColor(Editor::Color::ViewPortSelected),
                        Editor::GetIcon(Editor::AppIcons::EnvLight),
                        (uint32_t)e
                    );

                    HRay::SubmitSkyLight(ctx.rd, fd, skyLight, euler.y);
                }

                fd.sceneInfo.light.enableEnvironmentLight = view.size();
            }

            HRay::EndScene(ctx.rd, fd, ctx.commandList, { viewMatrix, projection, camPos, fov, (uint32_t)width, (uint32_t)height });
            Tiny2D::EndScene();

            {
                HE_ASSERT(compositeTarget);
                if ((width > 0 && height > 0) && (width != compositeTarget->getDesc().width || height != compositeTarget->getDesc().height))
                {
                    auto format = HRay::GetColorTarget(fd)->getDesc().format;
                    CreateOrResizeRenderTarget(this, format, width, height);
                }
            }

            {
                if (!compositeBindingSet)
                {
                    HE_VERIFY(HRay::GetColorTarget(fd));
                    HE_VERIFY(HRay::GetDepthTarget(fd));
                    HE_VERIFY(Tiny2D::GetColorTarget(tiny2DView));
                    HE_VERIFY(Tiny2D::GetDepthTarget(tiny2DView));

                    nvrhi::BindingSetDesc bindingSetDesc;
                    bindingSetDesc.bindings = {
                        nvrhi::BindingSetItem::Texture_SRV(0, Tiny2D::GetColorTarget(tiny2DView)),
                        nvrhi::BindingSetItem::Texture_SRV(1, Tiny2D::GetDepthTarget(tiny2DView)),
                        nvrhi::BindingSetItem::Texture_SRV(2, Tiny2D::GetEntitiesIDTarget(tiny2DView)),

                        nvrhi::BindingSetItem::Texture_SRV(3, HRay::GetColorTarget(fd)),
                        nvrhi::BindingSetItem::Texture_SRV(4, HRay::GetDepthTarget(fd)),
                        nvrhi::BindingSetItem::Texture_SRV(5, HRay::GetEntitiesIDTarget(fd)),

                        nvrhi::BindingSetItem::Texture_UAV(0, compositeTarget),
                        nvrhi::BindingSetItem::Texture_UAV(1, idTarget),
                    };

                    compositeBindingSet = ctx.device->createBindingSet(bindingSetDesc, compositeBindingLayout);
                }

                ctx.commandList->setComputeState({ computePipeline , { compositeBindingSet } });
                ctx.commandList->dispatch(width / 8, height / 8);

                pixelReadbackPass.Capture(ctx.commandList, idTarget, pixelPosition);
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
        auto viewportMinRegion = ImGui::GetCursorScreenPos();
        auto viewportMaxRegion = ImGui::GetCursorScreenPos() + ImGui::GetContentRegionAvail();
        auto viewportOffset = ImGui::GetWindowPos();

        auto& io = ImGui::GetIO();
        auto& style = ImGui::GetStyle();
        float dpiScale = ImGui::GetWindowDpiScale();
        float scale = ImGui::GetIO().FontGlobalScale * dpiScale;

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

        auto size = ImGui::GetContentRegionAvail();
        uint32_t w = std::max(8u, HE::AlignUp(uint32_t(size.x), 8u));
        uint32_t h = std::max(8u, HE::AlignUp(uint32_t(size.y), 8u));

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

        if (compositeTarget)
        {
            ImGui::Image(compositeTarget.Get(), size);
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

                    Math::float3 position, scale, skew;
                    Math::quat quaternion;
                    Math::vec4 perspective;
                    Math::decompose(entityLocalSpaceTransform, scale, quaternion, position, skew, perspective);

                    tc.position = position;
                    tc.rotation = quaternion;
                    tc.scale = scale;

                    clearReq |= true;
                }
            }

            auto hoveredEntity = GetHoveredEntity();
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing())
            {
                ctx.selectedEntity = hoveredEntity;
            }
        }

        // Toolbar
        {
            Editor::BeginChildView("Toolbar", toolbarCorner);

            {
                ImGui::ScopedFont sf(Editor::FontType::Blod, Editor::FontSize::Caption);
                ImGui::ScopedStyle ss(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.6f));
                ImGui::ScopedStyle ssis(ImGuiStyleVar_ItemSpacing, ImVec2(1.0f, 1.0f));
                ImVec2 buttonSize = ImVec2(24, 24) * io.FontGlobalScale + style.FramePadding * 2;

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
            }

            Editor::EndChildView();
        }

        // Right Toolbar
        {
            Editor::BeginChildView("Right Toolbar", axisGizmoCorner);

            ImGui::ScopedStyle sswp(ImGuiStyleVar_WindowPadding, ImVec2(3.0f, 3.0f));
            
            {
                ImVec2 buttonSize = ImVec2(18, 18) * io.FontGlobalScale + style.FramePadding * 2;

                ImGui::BeginChild("Overlay", { 0, 0 }, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
                
                {
                    ImGui::ScopedFont sf(Editor::FontType::Blod, Editor::FontSize::Caption);
                    ImGui::ScopedStyle ss(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.6f));

                    if (ImGui::SelectableButton(Icon_Mesh, buttonSize, openMeshDebuggingOverlay))
                    {
                        openMeshDebuggingOverlay = true;
                        ImGui::OpenPopup("Mesh Debugging Overlay");
                    }
                }

                {
                    ImGui::ScopedStyle ss(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                    ImGui::SetNextWindowSize({ 300 * io.FontGlobalScale, 0 });
                    if (ImGui::BeginPopup("Mesh Debugging Overlay"))
                    {
                        ImVec2 buttonSize = ImVec2(60, 16) * io.FontGlobalScale + style.FramePadding * 2;

                        {
                            ImGui::ScopedFont sf(Editor::FontType::Blod);
                            ImGui::TextUnformatted("Mesh Debugging Overlay");
                        }
                        ImGui::Dummy({ -1, 4 });

                        ImGui::ShiftCursorX((ImGui::GetContentRegionAvail().x - (buttonSize.x + 1) * 4) / 2);


                        if (ImGui::SelectableButton("Tangent", buttonSize, debug.enableMeshTangents))
                            debug.enableMeshTangents = debug.enableMeshTangents ? false : true;

                        ImGui::SameLine(0, 1);

                        if (ImGui::SelectableButton("Bitangent", buttonSize, debug.enableMeshBitangents))
                            debug.enableMeshBitangents = debug.enableMeshBitangents ? false : true;

                        ImGui::SameLine(0, 1);

                        if (ImGui::SelectableButton("Normal", buttonSize, debug.enableMeshNormals))
                            debug.enableMeshNormals = debug.enableMeshNormals ? false : true;

                        ImGui::SameLine(0, 1);

                        if (ImGui::SelectableButton("AABB", buttonSize, debug.enableMeshAABB))
                            debug.enableMeshAABB = debug.enableMeshAABB ? false : true;

                        ImGui::Dummy({ -1, 4 });

                        if (ImGui::BeginTable("Props", 2, ImGuiTableFlags_SizingFixedFit))
                        {
                            ImGui::ScopedCompact sc;

                            ImField::Checkbox("Stats", &debug.enableStats);
                            ImField::DragFloat("Line Length", &debug.lineLength, 0.001f, 0.01f, 1.0f);
                            ImField::DragFloat("Color Opacity", &debug.colorOpacity, 0.001f, 0.1f, 1.0f);

                            ImGui::EndTable();
                        }

                        ImGui::EndPopup();
                    }
                    else
                    {
                        openMeshDebuggingOverlay = false;
                    }
                }

                ImGui::SameLine(0, 1);

                {
                    ImGui::ScopedFont sf(Editor::FontType::Blod, Editor::FontSize::Caption);
                    ImGui::ScopedStyle ss(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.6f));

                    if (ImGui::SelectableButton(Icon_TV, buttonSize, openViewOverlay))
                    {
                        openViewOverlay = true;
                        ImGui::OpenPopup("View Overlay");
                    }
                }

                {
                    ImGui::ScopedStyle ss(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                    ImGui::SetNextWindowSize({ 300 * io.FontGlobalScale, 0 });

                    if (ImGui::BeginPopup("View Overlay"))
                    {
                        ImVec2 buttonSize = ImVec2(70, 16) * io.FontGlobalScale + style.FramePadding * 2;

                        {
                            ImGui::ScopedFont sf(Editor::FontType::Blod);
                            ImGui::TextUnformatted("View Overlay");
                        }

                        ImGui::Dummy({ -1, 4 });

                        {
                            ImGui::TextUnformatted("Ton Maper");

                            ImGui::ShiftCursorX((ImGui::GetContentRegionAvail().x - (buttonSize.x + 1) * 3) / 2);

                            bool isNone = fd.sceneInfo.postProssing.tonMappingType == HRay::TonMapingType::None;
                            if (ImGui::SelectableButton("None", buttonSize, isNone))
                                fd.sceneInfo.postProssing.tonMappingType = HRay::TonMapingType::None;

                            ImGui::SameLine(0, 1);

                            bool isWhatEver = fd.sceneInfo.postProssing.tonMappingType == HRay::TonMapingType::WhatEver;
                            if (ImGui::SelectableButton("WhatEver", buttonSize, isWhatEver))
                                fd.sceneInfo.postProssing.tonMappingType = HRay::TonMapingType::WhatEver;

                            ImGui::SameLine(0, 1);

                            bool isACES = fd.sceneInfo.postProssing.tonMappingType == HRay::TonMapingType::ACES;
                            if (ImGui::SelectableButton("ACES", buttonSize, isACES))
                                fd.sceneInfo.postProssing.tonMappingType = HRay::TonMapingType::ACES;


                            ImGui::ShiftCursorX((ImGui::GetContentRegionAvail().x - (buttonSize.x + 1) * 3) / 2);


                            bool isACESFitted = fd.sceneInfo.postProssing.tonMappingType == HRay::TonMapingType::ACESFitted;
                            if (ImGui::SelectableButton("ACESFitted", buttonSize, isACESFitted))
                                fd.sceneInfo.postProssing.tonMappingType = HRay::TonMapingType::ACESFitted;

                            ImGui::SameLine(0, 1);

                            bool isFilmic = fd.sceneInfo.postProssing.tonMappingType == HRay::TonMapingType::Filmic;
                            if (ImGui::SelectableButton("Filmic", buttonSize, isFilmic))
                                fd.sceneInfo.postProssing.tonMappingType = HRay::TonMapingType::Filmic;

                            ImGui::SameLine(0, 1);

                            bool isReinhard = fd.sceneInfo.postProssing.tonMappingType == HRay::TonMapingType::Reinhard;
                            if (ImGui::SelectableButton("Reinhard", buttonSize, isReinhard))
                                fd.sceneInfo.postProssing.tonMappingType = HRay::TonMapingType::Reinhard;
                        }

                        ImGui::Dummy({ -1, 8 });

                        {
                            ImGui::TextUnformatted("Rindering Mode");

                            ImGui::ShiftCursorX((ImGui::GetContentRegionAvail().x - (buttonSize.x + 1) * 1) / 2);

                            bool isPathTracing = fd.sceneInfo.settings.renderingMode == HRay::RenderingMode::PathTracing;
                            if (ImGui::SelectableButton("PathTracing", buttonSize, isPathTracing))
                            {
                                fd.sceneInfo.settings.renderingMode = HRay::RenderingMode::PathTracing;
                                HRay::Clear(fd);
                            }

                            ImGui::ShiftCursorX((ImGui::GetContentRegionAvail().x - (buttonSize.x + 1) * 3) / 2);

                            bool isNormals = fd.sceneInfo.settings.renderingMode == HRay::RenderingMode::Normals;
                            if (ImGui::SelectableButton("Normals", buttonSize, isNormals))
                            {
                                fd.sceneInfo.settings.renderingMode = HRay::RenderingMode::Normals;
                                HRay::Clear(fd);
                            }

                            ImGui::SameLine(0, 1);

                            bool isTangent = fd.sceneInfo.settings.renderingMode == HRay::RenderingMode::Tangent;
                            if (ImGui::SelectableButton("Tangent", buttonSize, isTangent))
                            {
                                fd.sceneInfo.settings.renderingMode = HRay::RenderingMode::Tangent;
                                HRay::Clear(fd);
                            }

                            ImGui::SameLine(0, 1);

                            bool isBitangent = fd.sceneInfo.settings.renderingMode == HRay::RenderingMode::Bitangent;
                            if (ImGui::SelectableButton("Bitangent", buttonSize, isBitangent))
                            {
                                fd.sceneInfo.settings.renderingMode = HRay::RenderingMode::Bitangent;
                                HRay::Clear(fd);
                            }
                        }

                        ImGui::EndPopup();
                    }
                    else
                    {
                        openViewOverlay = false;
                    }
                }

                ImGui::EndChild();
            }

            // ImGui::SameLine(0, 2);
            //{
            //    ImGui::ScopedFont sf(Editor::FontType::Blod, Editor::FontSize::Caption);
            //    ImGui::ScopedStyle ss(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.6f));
            //    ImVec2 buttonSize = ImVec2(18, 18) * io.FontGlobalScale + style.FramePadding * 2;
            //
            //    ImGui::BeginChild("Renderers", { 0, 0 }, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
            //
            //    if (ImGui::SelectableButton(Icon_Camera, buttonSize, true))
            //    {
            //        debug.enableMeshNormals = debug.enableMeshTangents = debug.enableMeshBitangents = debug.enableMeshAABB = false;
            //    }
            //
            //    ImGui::EndChild();
            //}
            
            Editor::EndChildView();
        }

        // Axis Gizmo
        {
            Editor::BeginChildView("Axis Gizmo", axisGizmoCorner, { 0, 32 });

            float size = 120 * scale;
            auto viewMatrix = Math::value_ptr(editorCamera->view.view);
            auto projMat = Math::value_ptr(Math::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f));

            if (ImGuizmo::ViewAxisWidget(viewMatrix, projMat, ImGui::GetCursorScreenPos(), size, 0.1f))
            {
                auto transform = Math::inverse(editorCamera->view.view);

                Math::float3 p, s, skew;
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
            if (debug.enableStats && tiny2DView)
            {
                auto& stats = Tiny2D::GetStats(tiny2DView);

                ImGui::Text("width/height %i / %i", compositeTarget->getDesc().width, compositeTarget->getDesc().height);
                ImGui::Text("lines %i | quads %i | boxes %i", stats.LineCount, stats.quadCount, stats.boxCount);
            }

            if (appStats.FPS < 30) ImGui::PushStyleColor(ImGuiCol_Text, GetColor(Color::Dangerous));
            ImGui::Text("CPUMain %.2f ms | FPS %i", appStats.CPUMainTime, appStats.FPS);
            if (appStats.FPS < 30) ImGui::PopStyleColor();

            ImGui::SameLine(0,2);
            ImGui::Text("| Sampels %i | Time %.2f s", fd.frameIndex, fd.time);

            Editor::EndChildView();
        }
    }
}

void Editor::ViewPortWindow::OnEnd(HE::Timestep ts) 
{
    selected = pixelReadbackPass.ReadUInt();
}

void Editor::ViewPortWindow::UpdateEditorCameraAnimation(Assets::Scene* scene, Assets::Entity mainCameraEntity, float ts)
{
    HE_ASSERT(scene);
    HE_ASSERT(mainCameraEntity);

    auto& ctx = Editor::GetContext();

    if (cameraAnimation.state & Editor::Animation::Animating)
    {
        auto wt = mainCameraEntity.GetWorldSpaceTransformMatrix();
        Math::float3 position, scale, skew;
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

    Math::float3 focalPoint = flyCamera->transform.position - flyCamera->transform.GetForward() * 10.0f;
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
        Math::float3 p, s, skew;
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

void Editor::ViewPortWindow::SetRendererToEditorCameraProp(HRay::FrameData& frameData)
{
    frameData.sceneInfo.view.minDistance = editorCamera->view.near;
    frameData.sceneInfo.view.maxDistance = editorCamera->view.far;
    frameData.sceneInfo.view.fov = editorCamera->view.fov;

    frameData.sceneInfo.view.enableDepthOfField = false;
    frameData.sceneInfo.view.enableVisualFocusDistance = false;
    frameData.sceneInfo.view.apertureRadius = 0;
    frameData.sceneInfo.view.focusFalloff = 0;
    frameData.sceneInfo.view.focusDistance = 10;
}

Assets::Entity Editor::ViewPortWindow::GetHoveredEntity()
{
    auto& ctx = GetContext();
    Assets::Scene* scene = ctx.assetManager.GetAsset<Assets::Scene>(ctx.sceneHandle);

    auto [mx, my] = ImGui::GetMousePos();
    mx -= viewportBounds[0].x;
    my -= viewportBounds[0].y;
    Math::vec2 viewportSize = viewportBounds[1] - viewportBounds[0];

    int mouseX = (int)mx;
    int mouseY = (int)my;

    if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
    {
        pixelPosition = { mouseX, mouseY };
        return { (entt::entity)selected, scene };
    }

    return {};
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

    out << "\t\t\"debug\" : {\n";
    {
        out << "\t\t\t\"enableMeshBitangents\" : " << (debug.enableMeshBitangents ? "true" : "false") << ",\n";
        out << "\t\t\t\"enableMeshTangents\" : " << (debug.enableMeshTangents ? "true" : "false") << ",\n";
        out << "\t\t\t\"enableMeshNormals\" : " << (debug.enableMeshNormals ? "true" : "false") << ",\n";
        out << "\t\t\t\"enableMeshAABB\" : " << (debug.enableMeshAABB ? "true" : "false") << ",\n";
        out << "\t\t\t\"enableStats\" : " << (debug.enableStats ? "true" : "false") << ",\n";
        out << "\t\t\t\"lineLength\" : " << debug.lineLength << ",\n";
        out << "\t\t\t\"colorOpacity\" : " << debug.colorOpacity << "\n";
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

    auto debugElement = element["debug"];
    if (!debugElement.error())
    {
        if (!debugElement["enableMeshBitangents"].error())
            debug.enableMeshBitangents = debugElement["enableMeshBitangents"].get_bool().value();

        if (!debugElement["enableMeshTangents"].error())
            debug.enableMeshTangents = debugElement["enableMeshTangents"].get_bool().value();

        if (!debugElement["enableMeshNormals"].error())
            debug.enableMeshNormals = debugElement["enableMeshNormals"].get_bool().value();

        if (!debugElement["enableMeshAABB"].error())
            debug.enableMeshAABB = debugElement["enableMeshAABB"].get_bool().value();

        if (!debugElement["enableStats"].error())
            debug.enableStats = debugElement["enableStats"].get_bool().value();

        if (!debugElement["lineLength"].error())
            debug.lineLength = (float)debugElement["lineLength"].get_double().value();

        if (!debugElement["colorOpacity"].error())
            debug.colorOpacity = (float)debugElement["colorOpacity"].get_double().value();
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

        auto rt = HRay::GetColorTarget(ctx.fd);
        if (rt)
        {
            ImVec2 availableSize = { (float)w, (float)h };
            float imageWidth = (float)rt->getDesc().width;
            float imageHeight = (float)rt->getDesc().height;
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

            ImGui::Image(rt, drawSize);
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

                            if (Editor::AssetPicker("Mesh Source", dm.meshSourceHandle, Assets::AssetType::MeshSource))
                            {
                                ms = ctx.assetManager.GetAsset<Assets::MeshSource>(dm.meshSourceHandle);
                                dm.meshIndex = dm.meshIndex < ms->meshes.size() ? dm.meshIndex : 0;
                            }

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
                if (ms && dm.meshIndex < ms->meshes.size())
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
                                Editor::AssetPicker("Bace Texture", mat->baseTextureHandle, Assets::AssetType::Texture2D, baseT, size);

                                if (ImField::ColorEdit4("Bace Color", &mat->baseColor.x)) Editor::Clear();

                                ImField::Separator();
                                auto normalT = ctx.assetManager.GetAsset<Assets::Texture>(mat->normalTextureHandle);
                                Editor::AssetPicker("Normal Texture", mat->normalTextureHandle, Assets::AssetType::Texture2D, normalT, size);

                                ImField::Separator();
                                auto metallicRoughnessT = ctx.assetManager.GetAsset<Assets::Texture>(mat->metallicRoughnessTextureHandle);
                              
                                Editor::AssetPicker("Metallic Roughness", mat->metallicRoughnessTextureHandle, Assets::AssetType::Texture2D, metallicRoughnessT, size);

                                if (ImField::DragFloat("Metallic", &mat->metallic, 0.01f, 0.0f, 1.0f)) Editor::Clear();
                                if (ImField::DragFloat("Roughness", &mat->roughness, 0.01f, 0.0f, 1.0f)) Editor::Clear();

                                ImField::Separator();
                                auto emissiveT = ctx.assetManager.GetAsset<Assets::Texture>(mat->emissiveTextureHandle);
                                Editor::AssetPicker("Emissive Texture", mat->emissiveTextureHandle, Assets::AssetType::Texture2D, emissiveT, size);

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

        if (ctx.selectedEntity.HasComponent<Assets::SkyLightComponent>())
        {
            if (ImField::BeginBlock("Sky Light", Icon_Sun, ctx.colors[Color::Light]))
            {
                auto& c = ctx.selectedEntity.GetComponent<Assets::SkyLightComponent>();

                if (ImGui::BeginTable("Sky Light Table", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    if (Editor::AssetPicker("Environment Map", c.textureHandle, Assets::AssetType::Texture2D))
                        c.totalSum = -1;

                    if (ImField::DragFloat("Intensity", &c.intensity)) Editor::Clear();

                    if (!c.textureHandle)
                    {
                        if (ImField::ColorEdit3("Ground Color", &c.groundColor.x)) Editor::Clear();
                        if (ImField::ColorEdit3("Horizon Sky Color", &c.horizonSkyColor.x)) Editor::Clear();
                        if (ImField::ColorEdit3("Zenith Sky Color", &c.zenithSkyColor.x)) Editor::Clear();
                    }

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
            DisplayAddComponentEntry<Assets::SkyLightComponent>(Icon_Sun"  Sky Light");
            DisplayAddComponentEntry<Assets::CameraComponent>(Icon_Camera"  Camera");

            ImGui::EndPopup();
        }
    }
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
            Editor::Clear();
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
    
            if (ImField::DragInt("Max Lighte Bounces", &ctx.fd.sceneInfo.settings.maxLighteBounces)) Editor::Clear();
            if (ImField::DragInt("Max Samples", &ctx.fd.sceneInfo.settings.maxSamples)) Editor::Clear();
            if (ImField::DragFloat("Exposure", &ctx.fd.sceneInfo.postProssing.exposure)) Editor::Clear();
            if (ImField::DragFloat("Gamma", &ctx.fd.sceneInfo.postProssing.gamma)) Editor::Clear();

            {
                int selected = 0;
                auto currentTypeStr = magic_enum::enum_name<HRay::TonMapingType>(ctx.fd.sceneInfo.postProssing.tonMappingType);
                auto types = magic_enum::enum_names<HRay::TonMapingType>();
                if (ImField::Combo("TonMapingType", types, currentTypeStr, selected))
                {
                    ctx.fd.sceneInfo.postProssing.tonMappingType = magic_enum::enum_cast<HRay::TonMapingType>(types[selected]).value();
                    
                    Editor::Clear();
                }
            }

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
                auto skyLightComponentCount = scene->registry.view<Assets::SkyLightComponent>().size();

                ImField::Text("Transform Component Count", "%zd", transformComponentCount);
                ImField::Text("Camera Component Count", "%zd", cameraComponentCount);
                ImField::Text("Mesh Component Count", "%zd", meshComponentCount);
                ImField::Text("Directional Light Component Count", "%zd", directionalLightComponentCount);
                ImField::Text("Sky Light Component Count", "%zd", skyLightComponentCount);
            }

            ImGui::EndTable();
        }
    }
    ImField::EndBlock();
}


#pragma endregion