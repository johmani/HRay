#include "Embeded/icon.h"
#include "Icons.h"
#include "HydraEngine/Base.h"

import HE;
import std;
import Math;
import nvrhi;
import ImGui;
import HRay;
import Assets;
import simdjson;
import magic_enum;
import Editor;

using namespace HE;

static void InitEntityFactory()
{
    auto Empty = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Empty", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        return newEntity;
    };

    auto Camera = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Camera", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);
        auto& cc = newEntity.AddComponent<Assets::CameraComponent>();

        return newEntity;
    };

    auto PlaneMesh = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Plane", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
        Assets::AssetHandle handle = Editor::GetAssetManager().GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

        auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, (int)Editor::Primitive::Plane);

        return newEntity;
    };

    auto CubeMesh = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Cube", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
        Assets::AssetHandle handle = Editor::GetAssetManager().GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

        auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, (int)Editor::Primitive::Cube);

        return newEntity;
    };

    auto CapsuleMesh = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Capsule", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
        Assets::AssetHandle handle = Editor::GetAssetManager().GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

        auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, (int)Editor::Primitive::Capsule);

        return newEntity;
    };

    auto UVSphereMesh = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "UVSphere", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
        Assets::AssetHandle handle = Editor::GetAssetManager().GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

        auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, (int)Editor::Primitive::UVSphere);

        return newEntity;
    };

    auto IcoSphereMesh = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "IcoSphere", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
        Assets::AssetHandle handle = Editor::GetAssetManager().GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

        auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, (int)Editor::Primitive::IcoSphere);

        return newEntity;
    };

    auto CylinderMesh = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Cylinder", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
        Assets::AssetHandle handle = Editor::GetAssetManager().GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

        auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, (int)Editor::Primitive::Cylinder);

        return newEntity;
     };

    auto ConeMesh = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Cone", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
        Assets::AssetHandle handle = Editor::GetAssetManager().GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

        auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, (int)Editor::Primitive::Cone);

        return newEntity;
    };

    auto TorusMesh = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Torus", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
        Assets::AssetHandle handle = Editor::GetAssetManager().GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

        auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, (int)Editor::Primitive::Torus);

        return newEntity;
    };

    auto Suzanne = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Suzanne", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);

        std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
        Assets::AssetHandle handle = Editor::GetAssetManager().GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

        auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, (int)Editor::Primitive::Suzanne);

        return newEntity;
    };

    auto DynamicSkyLight = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {
           
        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Dynamic Sky Light", scene->FindEntity(parent));
        Assets::Entity newEntity = scene->CreateEntity(name, parent);
    
        auto& dsl = newEntity.AddComponent<Assets::DynamicSkyLightComponent>();
    
        return newEntity;
    };


    auto DirectionalLight = [](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

        std::string name = Editor::GetIncrementedReletiveEntityName(scene, "Directional Light", scene->FindEntity(parent));
        auto newEntity = scene->CreateEntity(name, parent);
        auto& dl = newEntity.AddComponent<Assets::DirectionalLightComponent>();

        return newEntity;
    };

    //auto PointLight = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {
    //
    //    std::string name = GetIncrementedReletiveEntityName("Point Light", scene->FindEntity(parent));
    //    Assets::Entity newEntity = scene->CreateEntity(name, parent);
    // 
    //    auto& dl = newEntity.AddComponent<Assets::PointLightComponent>();
    //    
    //    return newEntity;
    //};

    //auto SpotLight = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {
    //
    //    std::string name = GetIncrementedReletiveEntityName(scene, "Spot Light", scene->FindEntity(parent));
    //    Assets::Entity newEntity = scene->CreateEntity(name, parent);
    // 
    //   auto& dl = newEntity.AddComponent<Assets::SpotLightComponent>();
    //    
    //    return newEntity;
    //};

    Editor::GetContext().createEnityFucntions = {

        { "Empty"                     , Empty },
        { "Camera"                    , Camera },
        { "Mesh/Plane"                , PlaneMesh },
        { "Mesh/Cube"                 , CubeMesh },
        { "Mesh/Capsule"              , CapsuleMesh },
        { "Mesh/UVSphere"             , UVSphereMesh },
        { "Mesh/IcoSphere"            , IcoSphereMesh },
        { "Mesh/Cylinder"             , CylinderMesh },
        { "Mesh/Cone"                 , ConeMesh },
        { "Mesh/Torus"                , TorusMesh },
        { "Mesh/Suzanne"              , Suzanne },
        { "Light/Dynamic Sky Light"   , DynamicSkyLight },
        { "Light/Directional"         , DirectionalLight },
        //{ "Light/Point"			      , PointLight },
        //{ "Light/Spot"			      , SpotLight },
    };
}

void Editor::App::OnAttach()
{
    HE_PROFILE_FUNCTION();

    Editor::App::s_Context = new Editor::Context();

    auto& ctx = Editor::GetContext();
    ctx.app = this;

    ctx.device = RHI::GetDevice();
    ctx.commandList = ctx.device->createCommandList();

    std::filesystem::path projecFilePath;

    // Paths
    {
        auto args = Application::GetApplicationDesc().commandLineArgs;
        projecFilePath = args.count == 2 ? args[1] : "";

        ctx.appData = FileSystem::GetAppDataPath(Application::GetApplicationDesc().windowDesc.title);
        ctx.keyBindingsFilePath = std::filesystem::current_path() / "Resources" / "keyBindings.json";
    }

    Plugins::LoadPluginsInDirectory("Plugins");
    Input::DeserializeKeyBindings(ctx.keyBindingsFilePath);

    {
        Assets::AssetManagerDesc desc;
        Editor::GetAssetManager().Init(ctx.device, desc);
        InitEntityFactory();
    }

    // icons
    {
        HE_PROFILE_SCOPE("Load Icons");

        auto cl = ctx.device->createCommandList({ .enableImmediateExecution = false });
        cl->open();
        {
            ctx.icon = Assets::LoadTexture(Application::GetApplicationDesc().windowDesc.iconFilePath, ctx.device, cl);
            ctx.close = Assets::LoadTexture(Buffer(g_icon_close, sizeof(g_icon_close)), ctx.device, cl);
            ctx.min = Assets::LoadTexture(Buffer(g_icon_minimize, sizeof(g_icon_minimize)), ctx.device, cl);
            ctx.max = Assets::LoadTexture(Buffer(g_icon_maximize, sizeof(g_icon_maximize)), ctx.device, cl);
            ctx.res = Assets::LoadTexture(Buffer(g_icon_restore, sizeof(g_icon_restore)), ctx.device, cl);
            ctx.board = Assets::LoadTexture(Buffer(g_icon_board, sizeof(g_icon_board)), ctx.device, cl);
        }
        cl->close();
        ctx.device->executeCommandList(cl);
    }

    // renderer
    {
        ctx.rd.am = &Editor::GetAssetManager();
        HRay::Init(ctx.rd, ctx.device, ctx.commandList);
    }

    {
        ctx.colors[Color::PrimaryButton] = { 0.278431f, 0.447059f, 0.701961f, 1.00f };
        ctx.colors[Color::TextButtonHovered] = { 0.278431f, 0.447059f, 0.701961f, 1.00f };
        ctx.colors[Color::TextButtonActive] = { 0.278431f, 0.447059f, 0.801961f, 1.00f };

        ctx.colors[Color::Dangerous] = { 0.8f, 0.3f, 0.2f, 1.0f };
        ctx.colors[Color::DangerousHovered] = { 0.9f, 0.2f, 0.2f, 1.0f };
        ctx.colors[Color::DangerousActive] = { 1.0f, 0.2f, 0.2f, 1.0f };

        ctx.colors[Color::Info] = { 0.278431f, 0.701961f, 0.447059f, 1.00f };
        ctx.colors[Color::Warn] = { 0.8f, 0.8f, 0.2f, 1.0f };
        ctx.colors[Color::Error] = { 0.8f, 0.3f, 0.2f, 1.0f };
        ctx.colors[Color::ChildBlock] = { 0.1f,0.1f ,0.1f ,1.0f };
        ctx.colors[Color::Selected] = { 0.3f, 0.3f, 0.3f , 1.0f };

        ctx.colors[Color::Mesh] = { 0.1f, 0.7f, 9.0f, 1.0f };
        ctx.colors[Color::Transform] = { 0.9f, 0.7f, 0.1f, 1.0f };
        ctx.colors[Color::Light] = { 0.9f, 0.7f, 0.1f, 1.0f };
        ctx.colors[Color::Material] = { 0.9f, 0.5f, 0.8f, 1.0f };
        ctx.colors[Color::Camera] = { 0.7f, 0.1f, 0.9f, 1.0f };
    }

    // Windows
    {
        Editor::BindWindow<ViewPortWindow>({
            .title = "View Port",
            .Icon = Icon_TV,
            .invokePath = "Window/General/View Port",
            .maxWindowInstances = 5
        });

        Editor::BindWindow<OutputWindow>({
           .title = "Output",
           .Icon = Icon_Film,
           .invokePath = "Window/General/Output",
        });

        Editor::BindWindow<HierarchyWindow>({
            .title = "Hierarchy",
            .Icon = Icon_Hierarchy,
            .invokePath = "Window/General/Hierarchy",
        });
    
        Editor::BindWindow<InspectorWindow>({
            .title = "Inspector",
            .Icon = Icon_InfoCircle,
            .invokePath = "Window/General/Inspector",
        });
    
        Editor::BindWindow<AssetManagerWindow>({
            .title = "Asset Manager",
            .Icon = Icon_InfoCircle,
            .invokePath = "Window/General/Asset Manager",
        });
    
        Editor::BindWindow<RendererSettingsWindow>({
            .title = "Renderer Settings",
            .Icon = Icon_Settings,
            .invokePath = "Window/General/Renderer Settings",
        });
    }

    OpenProject(projecFilePath);
}

void Editor::App::OnDetach()
{
    HE_PROFILE_FUNCTION();

    auto& ctx = GetContext();

    if (ctx.sceneMode == Editor::SceneMode::Runtime)
        Stop();

    Editor::Serialize();
    Editor::GetAssetManager().UnSubscribe(ctx.assetEventCallbackHandle);

    delete App::s_Context;
}

void Editor::App::OnEvent(Event& e)
{
    DispatchEvent<WindowDropEvent>(e, HE_BIND_EVENT_FN(Editor::App::OnWindowDropEvent));
}

bool Editor::App::OnWindowDropEvent(WindowDropEvent& e)
{
    HE_PROFILE_FUNCTION();

    for (int i = 0; i < e.count; i++)
    {
        std::filesystem::path file = e.paths[i];

        if (file.extension().string() == ".glb")
        {
            auto& ctx = GetContext();
            ImportModel(file);
        }
    }

    return true;
}

void Editor::App::OnAssetLoaded(Assets::Asset asset)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    auto type = Editor::GetAssetManager().GetAssetType(asset.GetHandle());
    switch (type)
    {
    case Assets::AssetType::MeshSource:
    {
        auto& meshSource = asset.Get<Assets::MeshSource>();

        if (ctx.importing)
        {
            auto& hierarchy = asset.Get<Assets::MeshSourecHierarchy>();
            Assets::Scene* scene = Editor::GetAssetManager().GetAsset<Assets::Scene>(ctx.sceneHandle);
            Editor::ImportMeshSource(scene, scene->GetRootEntity(), hierarchy.root, asset);
            ctx.importing = false;
        }

        Editor::Clear();

        break;
    }

    default: Editor::Clear();
    }
}

void Editor::App::OnAssetUnloaded(Assets::Asset asset)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = GetContext();

    auto type = Editor::GetAssetManager().GetAssetType(asset.GetHandle());
    switch (type)
    {
    case Assets::AssetType::Texture2D:

        HRay::ReleaseTexture(ctx.rd, &asset.Get<Assets::Texture>());

        break;
    }
}

void Editor::App::OnUpdate(const FrameInfo& info)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = GetContext();

    Assets::Scene* scene = Editor::GetAssetManager().GetAsset<Assets::Scene>(ctx.sceneHandle);

    if (scene && ctx.sceneMode == Editor::SceneMode::Runtime && (int)ctx.frameIndex < ctx.frameEnd)
    {
        Assets::Entity mainCameraEntity = Editor::GetSceneCamera(scene);

        if (mainCameraEntity)
        {
            Math::float3 camPos = {};
            Math::float4x4 viewMatrix;
            Math::float4x4 projection;

            auto& c = mainCameraEntity.GetComponent<Assets::CameraComponent>();
            auto wt = mainCameraEntity.GetWorldSpaceTransformMatrix();
            viewMatrix = Math::inverse(wt);

            Math::vec3 s, skew;
            Math::quat quaternion;
            Math::vec4 perspective;
            Math::decompose(wt, s, quaternion, camPos, skew, perspective);

            float aspectRatio = (float)ctx.width / (float)ctx.height;

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

            Editor::SetRendererToSceneCameraProp(ctx.fd, c);

            HRay::BeginScene(ctx.rd, ctx.fd);

            {
                auto& assetRegistry = Editor::GetAssetManager().registry;
                auto view = Editor::GetAssetManager().registry.view<Assets::Material>();
                for (auto e : view)
                {
                    Assets::Asset materalAsset = { e, &Editor::GetAssetManager() };
                    HRay::SubmitMaterial(ctx.rd, ctx.fd, materalAsset);
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
                        HRay::SubmitMesh(ctx.rd, ctx.fd, asset, mesh, wt, ctx.commandList);
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

                    HRay::SubmitDirectionalLight(ctx.rd, ctx.fd, light, wt);
                }
            }

            {
                auto view = scene->registry.view<Assets::DynamicSkyLightComponent>();
                for (auto e : view)
                {
                    Assets::Entity entity = { e, scene };
                    auto& dynamicSkyLight = entity.GetComponent<Assets::DynamicSkyLightComponent>();

                    HRay::SubmitSkyLight(ctx.rd, ctx.fd, dynamicSkyLight);
                }

                ctx.fd.sceneInfo.light.enableEnvironmentLight = view.size();
            }

            HRay::EndScene(ctx.rd, ctx.fd, ctx.commandList, { viewMatrix, projection, camPos, c.perspectiveFieldOfView, (uint32_t)ctx.width, (uint32_t)ctx.height });

            {
                ctx.sampleCount++;
                ctx.sampleCount = Math::min(ctx.sampleCount, ctx.maxSamples);

                if (ctx.sampleCount == ctx.maxSamples)
                {
                    for (int i = 0; i <= ctx.frameStep; i++)
                        OnUpdateFrame();

                    Editor::Save(ctx.device, ctx.fd.renderTarget, ctx.outputPath, ctx.frameIndex);

                    ctx.sampleCount = 0;
                    ctx.frameIndex += ctx.frameStep;
                    ctx.frameIndex = Math::min(ctx.frameIndex, ctx.frameEnd);

                    HRay::Clear(ctx.fd);
                    Editor::Clear();
                    if (ctx.frameIndex >= ctx.frameEnd)
                        Stop();
                }
            }
        }
    }

    bool validProject = std::filesystem::exists(ctx.project.projectFilePath);

    // Shortcuts
    if (true)
    {
        HE_PROFILE_SCOPE("Shortcuts");

        // App
        {
            if (Input::Triggered("Restart"))
                Application::Restart();

            if (Input::Triggered("Shutdown"))
                Application::Shutdown();

            if (Input::Triggered("ToggleScreenState"))
                Application::GetWindow().ToggleScreenState();

            if (Input::Triggered("ToggleWindowState"))
            {
                if (!Application::GetWindow().IsMaximize())
                    Application::GetWindow().Maximize();
                else
                    Application::GetWindow().Restore();
            }

            if (Input::Triggered("ToggleCursorState"))
            {
                if (Input::GetCursorMode() == Cursor::Mode::Disabled)
                    Input::SetCursorMode(Cursor::Mode::Normal);
                else
                    Input::SetCursorMode(Cursor::Mode::Disabled);
            }
        }

        // Windows
        {
            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::T))
                ctx.enableTitlebar = ctx.enableTitlebar ? false : true;
        }

        // Scene
        {
            if (validProject)
            {
                if (Input::IsKeyDown(Key::LeftControl) && Input::IsKeyPressed(Key::I))
                    OpenScene();

                if (Input::IsKeyDown(Key::LeftControl) && Input::IsKeyPressed(Key::N))
                    NewScene();

                if (Input::IsKeyDown(Key::LeftControl) && Input::IsKeyPressed(Key::S))
                    Editor::GetAssetManager().SaveAsset(ctx.sceneHandle);
            }
        }

        // Project
        {
            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::N))
                ctx.enableCreateNewProjectPopub = true;

            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::O))
            {
                auto file = FileDialog::OpenFile({ { "hray", "hray" } });
                OpenProject(file);
            }
        }
    }

    // UI
    {
        HE_PROFILE_SCOPE_NC("UI", 0xFF0000FF);

        ImGui::GetIO().FontGlobalScale = ctx.fontScale;
        auto& io = ImGui::GetIO();
        auto& style = ImGui::GetStyle();
        float dpiScale = ImGui::GetWindowDpiScale();
        float scale = ImGui::GetIO().FontGlobalScale * dpiScale;
        //style.FrameRounding = Math::max(3.0f * scale, 1.0f);

        ImVec2 mainWindowPadding = ImVec2(1, 1) * scale;
        ImVec2 mainItemSpacing = ImVec2(2, 2) * scale;
        ImVec2 headWindowPadding = ImVec2(18, 18) * scale;
        ImVec2 headFramePadding = ImVec2(14, 10) * scale;
        ImVec2 bodyWindowPadding = ImVec2(8, 8) * scale;
        ImVec2 bodyFramePadding = ImVec2(8, 10) * scale;
        ImVec2 cellPadding = ImVec2(20, 20) * scale;

        ImGui::ScopedStyle wb(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar);

        if (ctx.enableTitlebar)
        {
            bool customTitlebar = Application::GetApplicationDesc().windowDesc.customTitlebar;
            bool isIconClicked = Editor::BeginMainMenuBar(customTitlebar, ctx.icon.Get(), ctx.close.Get(), ctx.min.Get(), ctx.max.Get(), ctx.res.Get());

            {
                ImGui::SetNextWindowPos(ImGui::GetCurrentContext()->CurrentWindow->Pos + ImVec2(0, ImGui::GetCurrentContext()->CurrentWindow->Size.y));

                if (isIconClicked)
                {
                    ImGui::OpenPopup("Options");
                }

                if (ImGui::BeginPopup("Options"))
                {
                    {
                        ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2{ 1, 1 });

                        float w = 120 * scale;
                        {
                            ImGui::ScopedFont sf(FontType::Blod);
                            if (ImGui::TextButton("Options"))
                            {
                                ctx.fontScale = 1;
                                Editor::Serialize();
                            }
                        }

                        ImGui::Dummy({ w, 1 });

                        if (ImGui::TextButton("Font Scale"))
                        {
                            ctx.fontScale = 1;
                            Editor::Serialize();
                        }

                        ImGui::SameLine(0, w - ImGui::CalcTextSize("Font Scale").x);
                        ImGui::DragFloat("##Font Scale", &ctx.fontScale, 0.01f, 0.1f, 2.0f);
                        if (ImGui::IsItemDeactivatedAfterEdit()) { Editor::Serialize(); }
                    }

                    ImGui::EndPopup();
                }
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Restart", "Left Shift + ESC"))
                    Application::Restart();

                if (ImGui::MenuItem("Exit", "ESC"))
                    Application::Shutdown();

                if (ImGui::MenuItem("Full Screen", "F", Application::GetWindow().IsFullScreen()))
                    Application::GetWindow().ToggleScreenState();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Project", "Shift + N"))
                    ctx.enableCreateNewProjectPopub = true;

                if (ImGui::MenuItem("Open Project", "Shift + O"))
                {
                    auto file = FileDialog::OpenFile({ { "hray", "hray" } });
                    OpenProject(file);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("New Scene", "Ctrl + N", nullptr, validProject))
                    NewScene();

                if (ImGui::MenuItem("Open Scene", "Ctr + O", nullptr, validProject))
                    OpenScene();

                ImGui::Separator();

                if (ImGui::MenuItem("Import", "Ctr + I", nullptr, validProject))
                {
                    auto file = FileDialog::OpenFile({ { "model", "glb" } });
                    ImportModel(file);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::MenuItem("Title Bar", "Left Shift + T", ctx.enableTitlebar))
                    ctx.enableTitlebar = ctx.enableTitlebar ? false : true;
            
                if (ImGui::BeginMenu("Layout"))
                {
                    if (ImGui::MenuItem("Default", nullptr, nullptr, validProject))
                        Editor::LoadWindowsLayout();
            
                    ImGui::Separator();
            
                    if (ImGui::MenuItem("Close All Windows", nullptr, nullptr, validProject))
                        Editor::CloseAllWindows();
            
                    ImGui::EndMenu();
                }
            
                ImGui::EndMenu();
            }

            Editor::UpdateMenuItems();

            Editor::EndMainMenuBar();
        }

        if (ctx.enableStartMenu)
        {
            constexpr ImGuiWindowFlags fullScreenWinFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

            {
                const ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->WorkPos);
                ImGui::SetNextWindowSize(viewport->WorkSize);
                ImGui::Begin("StartMenu", nullptr, fullScreenWinFlags);

                const char* m = "Create New Project";
                const char* m1 = "Open Project";
                {
                    ImGui::ScopedFont sf(FontType::Blod, FontSize::Header1);
                    ImGui::ScopedColor  sc(ImGuiCol_Text, ImVec4(0.2f, 0.3f, 0.8f, 1.0f));

                    ImGui::ShiftCursor((ImGui::GetContentRegionAvail() - ImGui::CalcTextSize(m)) / 2);
                    if (ImGui::TextButton(m))
                    {
                        ctx.enableCreateNewProjectPopub = true;
                    }

                    ImGui::ShiftCursorX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(m1).x) / 2);
                    if (ImGui::TextButton(m1))
                    {
                        auto file = FileDialog::OpenFile({ { "hray", "hray" } });
                        OpenProject(file);
                    }
                }
                ImGui::End();
            }
        }
        else
        {
            Editor::UpdateWindows(info.ts);
        }

        if (ctx.enableCreateNewProjectPopub)
        {
            constexpr ImGuiWindowFlags fullScreenWinFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

            ImGui::SetNextWindowBgAlpha(0.8f);
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::Begin("new Project Fullscreen window", nullptr, fullScreenWinFlags);

            {
                ImGui::ScopedStyle wbs(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::ScopedStyle wr(ImGuiStyleVar_WindowRounding, 4.0f);
                ImGui::ScopedStyle scopedWindowPadding(ImGuiStyleVar_WindowPadding, mainWindowPadding);
                ImGui::ScopedStyle scopedItemSpacing(ImGuiStyleVar_ItemSpacing, mainItemSpacing);

                const ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->WorkPos + viewport->WorkSize * 0.1f / 2);
                ImGui::SetNextWindowSize(viewport->WorkSize * 0.9f);
                ImGui::Begin("CreateNewProject", nullptr, fullScreenWinFlags);

                if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
                    ctx.enableCreateNewProjectPopub = false;

                {
                    ImGui::ScopedStyle swp(ImGuiStyleVar_WindowPadding, ImVec2(8, 8) * scale);

                    ImGui::BeginChild("Creat New Project", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);

                    bool validPath = std::filesystem::exists(ctx.projectPath);
                    bool validName = !std::filesystem::exists(std::filesystem::path(ctx.projectPath) / ctx.projectName);

                    {
                        ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

                        {
                            ImGui::ScopedFont sf(FontType::Blod);
                            ImGui::TextUnformatted("Project Name");
                        }
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

                        if (!validName) ImGui::PushStyleColor(ImGuiCol_Text, ctx.colors[Color::Error]);
                        ImGui::InputTextWithHint("##Project Name", "Project Name", &ctx.projectName);
                        if (!validName) ImGui::PopStyleColor();
                    }

                    ImGui::Dummy({ -1, 10 });

                    {
                        ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

                        {
                            ImGui::ScopedFont sf(FontType::Blod);
                            ImGui::TextUnformatted("Location");
                        }
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50);

                        if (!validPath) ImGui::PushStyleColor(ImGuiCol_Text, ctx.colors[Color::Error]);
                        ImGui::InputTextWithHint("##Location", "Location", &ctx.projectPath);
                        if (!validPath) ImGui::PopStyleColor();

                        ImGui::SameLine();
                        if (ImGui::Button(Icon_Folder, { -1,0 }))
                        {
                            auto path = FileDialog::SelectFolder().string();
                            if (!path.empty())
                                ctx.projectPath = path;
                        }
                    }

                    ImGui::Dummy({ -1, 10 });

                    {
                        ImGui::ScopedFont sf(FontType::Blod);
                        ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

                        {
                            ImGui::ScopedDisabled sd(!validName || !validPath);
                            ImGui::ScopedButtonColor sbs(ctx.colors[Color::PrimaryButton]);
                            if (ImGui::Button("Create Project", { -1,0 }))
                            {
                                CreateNewProject(ctx.projectPath, ctx.projectName);
                                ctx.enableCreateNewProjectPopub = false;
                            }
                        }

                        if (ImGui::Button("Cancel", { -1,0 }))
                        {
                            ctx.enableCreateNewProjectPopub = false;
                        }
                    }

                    ImGui::EndChild();
                }

                ImGui::End();
            }
            ImGui::End();
        }
    }
}

void Editor::App::OnBegin(const FrameInfo& info)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = GetContext();

    ctx.commandList->open();
    nvrhi::utils::ClearColorAttachment(ctx.commandList, info.fb, 0, nvrhi::Color(0.1f));
}

void Editor::App::OnEnd(const FrameInfo& info)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = GetContext();

    ctx.commandList->close();
    ctx.device->executeCommandList(ctx.commandList);
}

void Editor::OnUpdateFrame()
{
    auto& ctx = GetContext();
#if 1
    Assets::Scene* scene = Editor::GetAssetManager().GetAsset<Assets::Scene>(ctx.sceneHandle);
    auto e = scene->FindEntity("dragon");
    if (e)
    {
        auto& r = e.GetTransform().rotation;
        r += Math::float3(0, 1 * 0.02f, 0);
    }
#endif
}

Editor::Context& Editor::GetContext() { return *App::s_Context; }

Assets::AssetManager& Editor::GetAssetManager() { return GetContext().assetManager; }

ImVec4 Editor::GetColor(int c) { return Editor::GetContext().colors[c]; }

Assets::Entity Editor::GetSceneCamera(Assets::Scene* scene)
{
    auto camView = scene->registry.view<Assets::CameraComponent>();
    for (auto e : camView)
    {
        Assets::Entity entity = { e, scene };
        auto& c = entity.GetComponent<Assets::CameraComponent>();
        if (c.isPrimary)
            return entity;
    }

    return {};
}

void Editor::SetRendererToSceneCameraProp(HRay::FrameData& frameData, const Assets::CameraComponent& c)
{
    frameData.sceneInfo.view.enableDepthOfField = c.depthOfField.enabled;
    frameData.sceneInfo.view.enableVisualFocusDistance = c.depthOfField.enableVisualFocusDistance && c.depthOfField.enabled;
    frameData.sceneInfo.view.apertureRadius = c.depthOfField.apertureRadius;
    frameData.sceneInfo.view.focusFalloff = c.depthOfField.focusFalloff;
    frameData.sceneInfo.view.focusDistance = c.depthOfField.focusDistance;
}

void Editor::SetRendererToEditorCameraProp(HRay::FrameData& frameData)
{
    frameData.sceneInfo.view.enableDepthOfField = false;
    frameData.sceneInfo.view.enableVisualFocusDistance = false;
    frameData.sceneInfo.view.apertureRadius = 0;
    frameData.sceneInfo.view.focusFalloff = 0;
    frameData.sceneInfo.view.focusDistance = 10;
}

void Editor::Animate()
{
    auto& ctx = Editor::GetContext();

    Assets::Scene* scene = Editor::GetAssetManager().GetAsset<Assets::Scene>(ctx.sceneHandle);
    if (!scene)
        return;

    Assets::Entity entity = Editor::GetSceneCamera(scene);
    if (!entity)
        return;

    Assets::AssetHandle handle;
    Assets::Asset asset = Editor::GetAssetManager().CreateAsset(handle);
    auto& assetState = asset.Get<Assets::AssetState>();
    auto& runtimeScene = asset.Add<Assets::Scene>();
    Assets::Scene::Copy(*scene, runtimeScene);
    Editor::GetAssetManager().MarkAsMemoryOnlyAsset(asset, Assets::AssetType::Scene);
    ctx.tempSceneHandle = ctx.sceneHandle;
    ctx.sceneHandle = handle;
    ctx.sampleCount = 0;
    ctx.frameIndex = 0;
    ctx.selectedEntity = {};

    Editor::Clear();
    ctx.sceneMode = SceneMode::Runtime;

    for (; ctx.frameIndex < ctx.frameStart; ctx.frameIndex++)
    {
#if 0
        OnUpdateFrame();
        HRay::Clear(ctx.rd);
#else
        HE::Jops::SubmitToMainThread([]() {

            auto& ctx = GetContext();

            Editor::OnUpdateFrame();
            Editor::Clear();
        });
#endif
    }

    // SceneCallback
    {
        auto& windowManager = ctx.windowManager;
        for (int i = 0; i < windowManager.scripts.size(); i++)
        {
            auto& script = windowManager.scripts[i];

            auto callback = dynamic_cast<Editor::SceneCallback*>(script.instance);
            if (callback)
            {
                callback->OnAnimationStart();
            }
        }
    }

    Editor::OpenWindow("Output");
    Editor::FocusWindow("Output");
}

void Editor::Stop()
{
    auto& ctx = Editor::GetContext();

    ctx.sceneMode = Editor::SceneMode::Editor;
    Editor::GetAssetManager().DestroyAsset(ctx.sceneHandle);
    ctx.sceneHandle = ctx.tempSceneHandle;
    ctx.sampleCount = 0;
    ctx.frameIndex = 0;
    ctx.selectedEntity = {};
    Editor::Clear();

    // SceneCallback
    {
        auto& windowManager = ctx.windowManager;
        for (int i = 0; i < windowManager.scripts.size(); i++)
        {
            auto& script = windowManager.scripts[i];

            auto callback = dynamic_cast<Editor::SceneCallback*>(script.instance);
            if (callback)
            {
                callback->OnAnimationStop();
            }
        }
    }

    Editor::FocusWindow("View Port");
}

void Editor::ImportModel(const std::filesystem::path& file)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    if (std::filesystem::exists(file))
    {
        ctx.importing = true;
        auto handle = Editor::GetAssetManager().GetOrMakeAsset(file, "Meshes" / file.filename());
        auto meshSource = Editor::GetAssetManager().GetAsset<Assets::MeshSource>(handle);
    }
}

void Editor::OpenProject(const std::filesystem::path& file)
{
    if (!std::filesystem::exists(file))
        return;

    auto& ctx = Editor::GetContext();

    {
        if (ctx.sceneMode == SceneMode::Runtime)
            Stop();

        ctx.selectedEntity = {};

        if (!ctx.project.projectFilePath.empty())
            Editor::Serialize();

        Editor::CloseAllWindows();
    }

    auto cacheDir = file.parent_path() / "Cache";
    ctx.project.projectFilePath = file;
    ctx.project.assetsDir = file.parent_path() / "Assets";
    ctx.project.cacheDir = cacheDir;
    ctx.project.assetsMetaDataFilePath = cacheDir / "assetsMetaData.json";
    
    ctx.project.layoutFilePath = (cacheDir / "layout.ini").lexically_normal().string();
    
    ctx.enableStartMenu = false;

    Editor::GetAssetManager().Reset();
    Editor::GetAssetManager().desc.assetsDirectory = ctx.project.assetsDir;
    Editor::GetAssetManager().desc.assetsRegistryFilePath = ctx.project.assetsMetaDataFilePath;
    ctx.assetEventCallbackHandle = Editor::GetAssetManager().Subscribe(Editor::GetContext().app);
    Editor::GetAssetManager().Deserialize();

    Editor::Deserialize();
}

void Editor::CreateNewProject(const std::filesystem::path& path, std::string projectName)
{
    auto& ctx = Editor::GetContext();

    {
        if (ctx.sceneMode == SceneMode::Runtime)
            Stop();

        ctx.selectedEntity = {};

        if (!ctx.project.projectFilePath.empty())
            Editor::Serialize();
       
        Editor::CloseAllWindows();
    }

    auto newProjectDir = path / projectName;
    auto cacheDir = newProjectDir / "Cache";
    ctx.project.cacheDir = cacheDir;
    ctx.project.projectFilePath = newProjectDir / std::format("{}.hray", projectName);
    ctx.project.assetsDir = newProjectDir / "Assets";
    ctx.project.assetsMetaDataFilePath = cacheDir / "assetsMetaData.json";
    
    ctx.project.layoutFilePath = (cacheDir / "layout.ini").lexically_normal().string();
    ctx.enableStartMenu = false;

    HE::FileSystem::ExtractZip(std::filesystem::current_path() / "Resources" / "Templates" / "Empty.zip", newProjectDir);
    HE::FileSystem::Rename(newProjectDir / "ProjectName.hray", ctx.project.projectFilePath);

    Editor::GetAssetManager().Reset();
    Editor::GetAssetManager().desc.assetsDirectory = ctx.project.assetsDir;
    Editor::GetAssetManager().desc.assetsRegistryFilePath = ctx.project.assetsMetaDataFilePath;
    ctx.assetEventCallbackHandle = Editor::GetAssetManager().Subscribe(Editor::GetContext().app);
    Editor::GetAssetManager().Deserialize();

    Editor::Deserialize();
}

void OpenStartMeue()
{
    auto& ctx = Editor::GetContext();
    ctx.enableStartMenu = true;
}

void Editor::OpenScene()
{
    HE_PROFILE_FUNCTION();

    auto file = FileDialog::OpenFile({ { "scene", "scene" } });

    if (std::filesystem::exists(file))
    {
        Jops::SubmitToMainThread([file]() {

            auto& ctx = Editor::GetContext();

            if (ctx.sceneMode == SceneMode::Runtime)
                Stop();

            auto assetFile = std::filesystem::relative(file, Editor::GetAssetManager().desc.assetsDirectory);
            ctx.sceneHandle = Editor::GetAssetManager().GetAssetHandleFromFilePath(assetFile);
            if (!Editor::GetAssetManager().IsAssetHandleValid(ctx.sceneHandle))
                ctx.sceneHandle = Editor::GetAssetManager().GetOrMakeAsset(file, assetFile);

            auto scene = Editor::GetAssetManager().GetAsset<Assets::Scene>(ctx.sceneHandle);
            HE_ASSERT(scene);
            Editor::Clear();
            Editor::Serialize();
        });
    }
}

void Editor::NewScene()
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    auto file = FileDialog::SaveFile({ { "scene", "scene" } });
    if (!file.empty())
    {
        if (ctx.sceneMode == SceneMode::Runtime)
            Stop();

        Editor::GetAssetManager().UnloadAllAssets();

        auto assetFile = std::filesystem::relative(file, Editor::GetAssetManager().desc.assetsDirectory);
        ctx.sceneHandle = Editor::GetAssetManager().CreateAsset(assetFile).GetHandle();
        Editor::Serialize();
    }
}

void Editor::Clear()
{
    auto& ctx = Editor::GetContext();
    auto& windowManager = ctx.windowManager;

    HRay::Clear(ctx.fd);

    for (auto s : windowManager.scripts)
    {
        if (s.instance)
        {
            auto view = dynamic_cast<Editor::ViewPortWindow*>(s.instance);
            if (view)
            {
                HRay::Clear(view->fd);
            }
        }
    }
}

Assets::Entity Editor::GetSelectedEntity() { return Editor::GetContext().selectedEntity; }

Assets::Scene* Editor::GetScene()
{
    auto& ctx = Editor::GetContext();

    return ctx.assetManager.GetAsset<Assets::Scene>(ctx.sceneHandle);
}

void Editor::Save(nvrhi::IDevice* device, nvrhi::ITexture* texture, const std::string& directory, uint32_t frameIndex)
{
    if (!std::filesystem::exists(directory))
    {
        HE_ERROR("Invalid Path {}", directory);
        return;
    }

    auto filePath = std::format("{}/{}.png", directory, frameIndex);

    auto textureState = nvrhi::ResourceStates::UnorderedAccess;
    const nvrhi::TextureDesc& desc = texture->getDesc();

    auto commandList = device->createCommandList({ .enableImmediateExecution = true });
    commandList->open();

    if (textureState != nvrhi::ResourceStates::Unknown)
    {
        commandList->beginTrackingTextureState(texture, nvrhi::TextureSubresourceSet(0, 1, 0, 1), textureState);
    }

    nvrhi::StagingTextureHandle stagingTexture = device->createStagingTexture(desc, nvrhi::CpuAccessMode::Read);
    HE_VERIFY(stagingTexture);

    commandList->copyTexture(stagingTexture, nvrhi::TextureSlice(), texture, nvrhi::TextureSlice());

    if (textureState != nvrhi::ResourceStates::Unknown)
    {
        commandList->setTextureState(texture, nvrhi::TextureSubresourceSet(0, 1, 0, 1), textureState);
        commandList->commitBarriers();
    }

    commandList->close();
    device->executeCommandList(commandList);

    HE::Jops::SubmitTask([device, filePath, desc, stagingTexture]() {

        size_t rowPitch = 0;
        void* pData = device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch);
        HE_VERIFY(pData);

        const auto& texDesc = stagingTexture->getDesc();
        uint32_t width = texDesc.width;
        uint32_t height = texDesc.height;

        if (texDesc.format != nvrhi::Format::RGBA16_UNORM)
        {
            HE_ERROR("Texture format is not RGBA16_UNORM");
            device->unmapStagingTexture(stagingTexture);
            return;
        }

        std::vector<uint8_t> rgba8Data(width * height * 4); // 4 bytes per pixel (RGBA8)
        const uint16_t* src = (const uint16_t*)pData;
        for (uint32_t y = 0; y < height; y++)
        {
            const uint16_t* rowSrc = src + (y * rowPitch / sizeof(uint16_t));
            for (uint32_t x = 0; x < width; x++)
            {
                uint32_t pixelIdx = (y * width + x) * 4;
                float gamma = 2.2f;
                float invGamma = 1.0f / gamma;
                rgba8Data[pixelIdx + 0] = (uint8_t)(255.0f * pow((float)rowSrc[x * 4 + 0] / 65535.0f, invGamma)); // R
                rgba8Data[pixelIdx + 1] = (uint8_t)(255.0f * pow((float)rowSrc[x * 4 + 1] / 65535.0f, invGamma)); // G
                rgba8Data[pixelIdx + 2] = (uint8_t)(255.0f * pow((float)rowSrc[x * 4 + 2] / 65535.0f, invGamma)); // B
                rgba8Data[pixelIdx + 3] = (uint8_t)(rowSrc[x * 4 + 3] >> 8);                                      // A
            }
        }

        // Write to PNG file
        int result = HE::Image::SaveAsPNG(filePath.c_str(), width, height, 4, rgba8Data.data(), width * 4);
        if (!result)
        {
            HE_ERROR("Failed to write PNG file: {}", filePath);
        }
        else
        {
            HE_INFO("Successfully wrote PNG file: {}", filePath);
        }

        device->unmapStagingTexture(stagingTexture);
    });
}

bool Editor::IsEntityNameExistInChildren(Assets::Scene* scene, const std::string& name, Assets::Entity entity)
{
    for (auto id : entity.GetChildren())
    {
        Assets::Entity entity = scene->FindEntity(id);
        if (entity.GetName() == name)
            return true;
    }

    return false;
}

std::string Editor::GetIncrementedReletiveEntityName(Assets::Scene* scene, const std::string& name, Assets::Entity parent)
{
    if (!IsEntityNameExistInChildren(scene, name, parent))
        return  name;

    std::string result = IncrementString(name);
    return GetIncrementedReletiveEntityName(scene, result, parent); // keep Incrementing the number
}

Assets::Entity Editor::CreateEntity(Assets::Scene* scene, const std::string& key, Assets::UUID parent)
{
    auto& ctx = Editor::GetContext();

    if (ctx.createEnityFucntions.contains(key))
        return ctx.createEnityFucntions.at(key)(scene, parent);

    return {};
}

void Editor::AddEntityFactory(const std::string& key, std::function<Assets::Entity(Assets::Scene* scene, Assets::UUID)> function)
{
    auto& ctx = Editor::GetContext();
    ctx.createEnityFucntions[key] = function;
}

void Editor::Serialize()
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    std::ofstream file(ctx.project.projectFilePath);
    std::ostringstream out;
    out << "{\n";

    // scene
    {
        out << "\t\"sceneHandle\" : " << ctx.sceneHandle << ",\n";
    }

    {
        Editor::SerializeWindows(out);
    }

    // main
    {
        out << "\t\"main\" : {\n";
        {
            out << "\t\t\"frameStart\" : " << ctx.frameStart << ",\n";
            out << "\t\t\"frameEnd\" : " << ctx.frameEnd << ",\n";
            out << "\t\t\"frameStep\" : " << ctx.frameStep << ",\n";
            out << "\t\t\"maxSamples\" : " << ctx.maxSamples << ",\n";
            out << "\t\t\"width\" : " << ctx.width << ",\n";
            out << "\t\t\"height\" : " << ctx.height << ",\n";

            out << "\t\t\"fontScale\" : " << ctx.fontScale << ",\n";
            out << "\t\t\"enableTitlebar\" : " << (ctx.enableTitlebar ? "true" : "false") << "\n";
        }
        out << "\t}\n";
    }

    out << "}\n";

    file << out.str();
    file.close();

    Editor::SerializeWindowsState();
}

bool Editor::Deserialize()
{
    auto& ctx = Editor::GetContext();

    HE_PROFILE_FUNCTION();

    if (!std::filesystem::exists(ctx.project.projectFilePath))
    {
        HE_ERROR("HRAy::Deserialize : Unable to open file for reaading, {}", ctx.project.projectFilePath.string());
        return false;
    }

    static simdjson::dom::parser parser;
    auto doc = parser.load(ctx.project.projectFilePath.string());
    
    // scene
    {
        auto sceneHandleData = doc["sceneHandle"];
        if (!sceneHandleData.error())
            ctx.sceneHandle = sceneHandleData.get_uint64().value();
    }

    if(!doc.error())
        Editor::DeserializeWindows(doc.value());

    auto main = doc["main"];
    if (!main.error())
    {
        {
            auto frameStart = main["frameStart"];
            if (!frameStart.error())
                ctx.frameStart = (int)frameStart.get_int64().value();
        }

        {
            auto frameEnd = main["frameEnd"];
            if (!frameEnd.error())
                ctx.frameEnd = (int)frameEnd.get_int64().value();
        }

        {
            auto frameStep = main["frameStep"];
            if (!frameStep.error())
                ctx.frameStep = (int)frameStep.get_int64().value();
        }

        {
            auto maxSamples = main["maxSamples"];
            if (!maxSamples.error())
                ctx.maxSamples = (int)maxSamples.get_int64().value();
        }

        {
            auto width = main["width"];
            if (!width.error())
                ctx.width = (int)width.get_int64().value();
        }

        {
            auto height = main["height"];
            if (!height.error())
                ctx.height = (int)height.get_int64().value();
        }
        
        {
            auto fontScale = main["fontScale"];
            if (!fontScale.error()) 
                ctx.fontScale = (float)fontScale.get_double().value();
        }

        {
            auto enableTitlebar = main["enableTitlebar"];
            if (!enableTitlebar.error())
                ctx.enableTitlebar = enableTitlebar.get_bool().value();
        }
    }

    Editor::DeserializeWindowsState();

    return true;
}


HE::ApplicationContext* HE::CreateApplication(ApplicationCommandLineArgs args)
{
    ApplicationDesc desc;
    desc.commandLineArgs = args;

#ifdef HE_DIST
    if (args.count == 2)
        desc.workingDirectory = std::filesystem::path(args[0]).parent_path();
#endif

#if HE_DEBUG
    desc.deviceDesc.enableGPUValidation = true;
    desc.deviceDesc.enableDebugRuntime = true;
    desc.deviceDesc.enableNvrhiValidationLayer = true;
#endif

    desc.deviceDesc.enableRayTracingExtensions = true;
    desc.deviceDesc.enableComputeQueue = true;
    desc.deviceDesc.enableCopyQueue = true;
    desc.deviceDesc.api = {
        nvrhi::GraphicsAPI::D3D12,
        nvrhi::GraphicsAPI::VULKAN,
    };

    desc.windowDesc.title = "HRay";
    desc.windowDesc.iconFilePath = "Resources/Icons/HRay.png";
    desc.windowDesc.customTitlebar = true;
    desc.windowDesc.minWidth = 960;
    desc.windowDesc.minHeight = 540;
    desc.windowDesc.maximized = true;
    desc.windowDesc.swapChainDesc.swapChainFormat = nvrhi::Format::SRGBA8_UNORM;

    auto log = (FileSystem::GetAppDataPath(desc.windowDesc.title) / desc.windowDesc.title).string();
    desc.logFile = log.c_str();

    ApplicationContext* ctx = new ApplicationContext(desc);
    Application::PushLayer(new Editor::App());

    return ctx;
}

#include "HydraEngine/EntryPoint.h"
