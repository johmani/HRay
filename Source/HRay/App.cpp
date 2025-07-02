#include "HydraEngine/Base.h"
#include <format>
#include "Embeded/icon.h"
#include "IconsFontAwesome.h"

import HE;
import std;
import Math;
import nvrhi;
import ImGui;
import Editor;
import HRay;
import Assets;
import simdjson;
import magic_enum;

using namespace HE;

struct Color {

    enum
    {
        PrimaryButton,
        Selected,

        TextButtonHovered,
        TextButtonActive,

        Dangerous,
        DangerousHovered,
        DangerousActive,

        Info,
        Warn,
        Error,

        ChildBlock,

        Count
    };
};

struct FontType
{
    enum : uint8_t
    {
        Regular = 0,
        Blod = 1,
    };
};

struct FontSize
{
    enum : uint8_t
    {
        Header0 = 36,
        Header1 = 28,
        Header2 = 24,
        BodyLarge = 20,
        BodyMedium = 18,
        BodySmall = 16,
        Caption = 13,
    };
};

#define Icon_Box           ICON_FA_BOX
#define Icon_DICE          ICON_FA_DICE_D6
#define Icon_Hierarchy     ICON_FA_SITEMAP
#define Icon_InfoCircle    ICON_FA_INFO_CIRCLE
#define Icon_Arros         ICON_FA_ARROWS_ALT
#define Icon_Mesh          ICON_FA_DHARMACHAKRA
#define Icon_Search        ICON_FA_SEARCH
#define Icon_Arrow         ICON_FA_LOCATION_ARROW
#define Icon_Move          ICON_FA_ARROWS_ALT
#define Icon_Rotate        ICON_FA_SYNC_ALT
#define Icon_Scale         ICON_FA_EXPAND_ALT
#define Icon_Transform     ICON_FA_EXPAND_ARROWS_ALT
#define Icon_Download      ICON_FA_DOWNLOAD
#define Icon_CloudDownload ICON_FA_CLOUD_DOWNLOAD_ALT
#define Icon_Cube          ICON_FA_CUBE
#define Icon_Plus          ICON_FA_PLUS
#define Icon_Trash         ICON_FA_TRASH_ALT
#define Icon_Recycle       ICON_FA_RECYCLE
#define Icon_Warning       ICON_FA_EXCLAMATION_TRIANGLE
#define Icon_X             ICON_FA_TIMES
#define Icon_CheckCircle   ICON_FA_CHECK_CIRCLE
#define Icon_FolderOpen    ICON_FA_FOLDER_OPEN
#define Icon_Folder        ICON_FA_FOLDER
#define Icon_Plugin        ICON_FA_PLUG
#define Icon_Settings      ICON_FA_BAHAI
#define Icon_Build         ICON_FA_HAMMER
#define Icon_Code          ICON_FA_CODE

struct Project
{
    std::filesystem::path projectFilePath;
    std::filesystem::path assetsMetaDataFilePath;
    std::filesystem::path assetsDir;
};

struct HRayApp : public Layer, public Assets::AssetEventCallback
{
    // UI
    ImVec4 colors[Color::Count];
    ImVec4 meshColor = { 0.1f, 0.7f, 9.0f, 1.0f };
    ImVec4 transformColor = { 0.9f, 0.7f, 0.1f, 1.0f };

    // App
    Ref<Editor::OrbitCamera> orbitCamera;
    Ref<Editor::FlyCamera> flyCamera;
    Ref<Editor::EditorCamera> editorCamera;
    int width = 1920, height = 1080;
    Math::vec2 viewportBounds[2];
    Assets::Entity selectedEntity;
    bool useViewportSize = true;
    bool importing = false;
    int gizmoType = ImGuizmo::TRANSLATE;
    const char* c_AppName = "HRay";
    bool enableTitlebar = true;
    bool enableViewPortWindow = true;
    bool enableHierarchyWindow = true;
    bool enableInspectorWindow = true;
    bool enableAssetManagerWindow = true;
    bool enableApplicationWindow = true;
    bool enableAdvancedMode = false;
    bool enableStartMenu = true;
    bool enableCreateNewProjectPopub = false;
    ImGuiTextFilter textFilter;

    std::string projectPath;
    std::string projectName;
    std::filesystem::path appData;
    std::filesystem::path keyBindingsFilePath;
    Project project;

    nvrhi::DeviceHandle device;
    nvrhi::CommandListHandle commandList;
    nvrhi::TextureHandle icon, close, min, max, res;

    Assets::AssetManager assetManager;
    Assets::SubscriberHandle AssetEventCallbackHandle = 0;
    Assets::AssetHandle sceneHandle = 0;
    HRay::RendererData rd;

    void OnAttach() override
    {
        HE_PROFILE_FUNCTION();

        device = RHI::GetDevice();
        commandList = device->createCommandList();

        // Paths
        {
            auto args = Application::GetApplicationDesc().commandLineArgs;
            project.projectFilePath = args.count == 2 ? args[1] : "";

            appData = FileSystem::GetAppDataPath(c_AppName);
            keyBindingsFilePath = std::filesystem::current_path() / "Resources" / "keyBindings.json";
        }

        // Camera 
        {
            orbitCamera = CreateScope<Editor::OrbitCamera>(60.0f, float(width) / float(height), 0.1f, 1000.0f);
            flyCamera = CreateScope<Editor::FlyCamera>(60.0f, float(width) / float(height), 0.1f, 1000.0f);
            editorCamera = orbitCamera;
        }
        
        Plugins::LoadPluginsInDirectory("Plugins");
        Input::DeserializeKeyBindings(keyBindingsFilePath);

        {
            Assets::AssetManagerDesc desc;
            assetManager.Init(device, desc);
            OpenProject(project.projectFilePath);
        }

        // icons
        {
            HE_PROFILE_SCOPE("Load Icons");

            auto cl = device->createCommandList({ .enableImmediateExecution = false });
            cl->open();
            {
                icon = Assets::LoadTexture(Application::GetApplicationDesc().windowDesc.iconFilePath, device, cl);
                close = Assets::LoadTexture(Buffer(g_icon_close, sizeof(g_icon_close)), device, cl);
                min = Assets::LoadTexture(Buffer(g_icon_minimize, sizeof(g_icon_minimize)), device, cl);
                max = Assets::LoadTexture(Buffer(g_icon_maximize, sizeof(g_icon_maximize)), device, cl);
                res = Assets::LoadTexture(Buffer(g_icon_restore, sizeof(g_icon_restore)), device, cl);
            }
            cl->close();
            device->executeCommandList(cl);
        }

        // renderer
        {
            rd.am = &assetManager;
            HRay::Init(rd, device, commandList);
        }

        {
            colors[Color::PrimaryButton] = { 0.278431f, 0.447059f, 0.701961f, 1.00f };
            colors[Color::TextButtonHovered] = { 0.278431f, 0.447059f, 0.701961f, 1.00f };
            colors[Color::TextButtonActive] = { 0.278431f, 0.447059f, 0.801961f, 1.00f };
            colors[Color::Dangerous] = { 0.8f, 0.3f, 0.2f, 1.0f };
            colors[Color::DangerousHovered] = { 0.9f, 0.2f, 0.2f, 1.0f };
            colors[Color::DangerousActive] = { 1.0f, 0.2f, 0.2f, 1.0f };
            colors[Color::Info] = { 0.278431f, 0.701961f, 0.447059f, 1.00f };
            colors[Color::Warn] = { 0.8f, 0.8f, 0.2f, 1.0f };
            colors[Color::Error] = { 0.8f, 0.3f, 0.2f, 1.0f };
            colors[Color::ChildBlock] = { 0.1f,0.1f ,0.1f ,1.0f };
            colors[Color::Selected] = { 0.3f, 0.3f, 0.3f , 1.0f };

            std::string layoutPath = (appData / "layout.ini").lexically_normal().string();
            ImGui::GetIO().IniFilename = nullptr;
            ImGui::LoadIniSettingsFromDisk(layoutPath.c_str());
        }
    }

    void OnDetach() override
    {
        HE_PROFILE_FUNCTION();

        Serialize();
        assetManager.UnSubscribe(AssetEventCallbackHandle);
    }

    void OnEvent(Event& e) override
    {
        DispatchEvent<WindowDropEvent>(e, HE_BIND_EVENT_FN(OnWindowDropEvent));
    }

    bool OnWindowDropEvent(WindowDropEvent& e)
    {
        HE_PROFILE_FUNCTION();

        for (int i = 0; i < e.count; i++)
        {
            std::filesystem::path file = e.paths[i];

            if (file.extension().string() == ".glb")
            {
                ImportModel(file);
                enableStartMenu = false;
            }
        }

        return true;
    }

    void OnAssetLoaded(Assets::Asset asset) override
    {
        HE_PROFILE_FUNCTION();

        auto type = assetManager.GetAssetType(asset.GetHandle());
        switch (type)
        {
        case Assets::AssetType::MeshSource:

            auto& meshSource = asset.Get<Assets::MeshSource>();

            if (importing)
            {
                auto& hierarchy = asset.Get<Assets::MeshSourecHierarchy>();
                Assets::Scene* scene = assetManager.GetAsset<Assets::Scene>(sceneHandle);
                Editor::ImportMeshSource(scene, scene->GetRootEntity(), hierarchy.root, asset);
                importing = false;
            }

            break;
        }
    }

    void OnAssetUnloaded(Assets::Asset asset) override
    {
        HE_PROFILE_FUNCTION();

        auto type = assetManager.GetAssetType(asset.GetHandle());
        switch (type)
        {
        case Assets::AssetType::Texture2D:

            HRay::ReleaseTexture(rd, &asset.Get<Assets::Texture>());

            break;
        }
    }

    void OnUpdate(const FrameInfo& info) override
    {
        HE_PROFILE_FUNCTION();

        bool validProject = std::filesystem::exists(project.projectFilePath);

        {
            HE_PROFILE_SCOPE("Render");

            Assets::Scene* scene = assetManager.GetAsset<Assets::Scene>(sceneHandle);
            if (scene)
            {
                HRay::BeginScene(rd);

                auto view = scene->registry.view<Assets::MeshComponent>();
                for (auto e : view)
                {
                    Assets::Entity entity = { e, scene };
                    auto& dm = entity.GetComponent<Assets::MeshComponent>();
                    auto wt = entity.GetWorldSpaceTransformMatrix();

                    auto asset = assetManager.GetAsset(dm.meshSourceHandle);
                    if (asset && asset.Has<Assets::MeshSource>() && asset.GetState() == Assets::AssetState::Loaded)
                    {
                        auto& meshSource = asset.Get<Assets::MeshSource>();
                        auto& mesh = meshSource.meshes[dm.meshIndex];

                        HRay::SubmitMesh(rd, asset, mesh, wt, commandList);
                    }
                }

                HRay::EndScene(rd, commandList, { editorCamera->view.view, editorCamera->view.projection, editorCamera->transform.position, (uint32_t)width, (uint32_t)height });
            }
        }

        // Shortcuts
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
                    enableTitlebar = enableTitlebar ? false : true;

                if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::D1))
                    enableViewPortWindow = enableViewPortWindow ? false : true;

                if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::D2))
                    enableHierarchyWindow = enableHierarchyWindow ? false : true;

                if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::D3))
                    enableInspectorWindow = enableInspectorWindow ? false : true;

                if (enableAdvancedMode)
                {
                    if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::D4))
                        enableAssetManagerWindow = enableAssetManagerWindow ? false : true;

                    if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::D5))
                        enableApplicationWindow = enableApplicationWindow ? false : true;
                }

                if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::D0))
                    enableAdvancedMode = enableAdvancedMode ? false : true;
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
                        assetManager.SaveAsset(sceneHandle);

                    if (Input::IsKeyDown(Key::LeftAlt) && Input::IsKeyPressed(Key::D1))
                        SetOrbitCamera();

                    if (Input::IsKeyDown(Key::LeftAlt) && Input::IsKeyPressed(Key::D2))
                        SetFlyCamera();

                    if (Input::IsKeyDown(Key::LeftControl) && Input::IsKeyReleased(Key::F))
                        FocusCamera();

                    if (Input::IsKeyReleased(Key::Delete))
                    {
                        Assets::Scene* scene = assetManager.GetAsset<Assets::Scene>(sceneHandle);
                        if (scene) scene->DestroyEntity(selectedEntity);
                    }
                }
            }

            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::N))
                enableCreateNewProjectPopub = true;

            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::O))
            {
                auto file = FileDialog::OpenFile({ { "hray", "hray" } });
                OpenProject(file);
            }
        }

        // UI
        {
            HE_PROFILE_SCOPE_NC("UI", 0xFF0000FF);

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

            if (enableTitlebar)
            {
                bool customTitlebar = Application::GetApplicationDesc().windowDesc.customTitlebar;
                bool isIconClicked = Editor::BeginMainMenuBar(customTitlebar, icon.Get(), close.Get(), min.Get(), max.Get(), res.Get());

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
                                    ImGui::GetIO().FontGlobalScale = 1;
                                    Serialize();
                                }
                            }

                            ImGui::Dummy({ w, 1 });

                            if (ImGui::TextButton("Font Scale"))
                            {
                                ImGui::GetIO().FontGlobalScale = 1;
                                Serialize();
                            }

                            ImGui::SameLine(0, w - ImGui::CalcTextSize("Font Scale").x);
                            ImGui::DragFloat("##Font Scale", &ImGui::GetIO().FontGlobalScale, 0.01f, 0.1f, 2.0f);
                            if (ImGui::IsItemDeactivatedAfterEdit()) { Serialize(); }
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
                        enableCreateNewProjectPopub = true;

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
                    if (ImGui::MenuItem("Title Bar", "Left Shift + T", enableTitlebar))
                        enableTitlebar = enableTitlebar ? false : true;

                    if (ImGui::MenuItem("View Port", "Left Shift + 1", enableViewPortWindow))
                        enableViewPortWindow = enableViewPortWindow ? false : true;

                    if (ImGui::MenuItem("Hierarchy", "Left Shift + 2", enableHierarchyWindow))
                        enableHierarchyWindow = enableHierarchyWindow ? false : true;

                    if (ImGui::MenuItem("Inspector", "Left Shift + 3", enableInspectorWindow))
                        enableInspectorWindow = enableInspectorWindow ? false : true;

                    if (enableAdvancedMode)
                    {
                        if (ImGui::MenuItem("Asset Manager", "Left Shift + 4", enableAssetManagerWindow))
                            enableAssetManagerWindow = enableAssetManagerWindow ? false : true;

                        if (ImGui::MenuItem("Application", "Left Shift + 5", enableApplicationWindow))
                            enableApplicationWindow = enableApplicationWindow ? false : true;
                    }

                    if (ImGui::MenuItem("Advanced Mode", "Left Shift + 0", enableAdvancedMode))
                        enableAdvancedMode = enableAdvancedMode ? false : true;

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("View"))
                {
                    if (ImGui::BeginMenu("Navegation"))
                    {
                        if (ImGui::MenuItem("Fly Camera", "Alt + 2", (bool)std::dynamic_pointer_cast<Editor::FlyCamera>(editorCamera)))
                            SetFlyCamera();

                        if (ImGui::MenuItem("Orbit Camera", "Alt + 1", (bool)std::dynamic_pointer_cast<Editor::OrbitCamera>(editorCamera)))
                            SetOrbitCamera();

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenu();
                }

                Editor::EndMainMenuBar();
            }

            if (enableStartMenu)
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
                            enableCreateNewProjectPopub = true;

                        ImGui::ShiftCursorX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(m1).x) / 2);
                        if (ImGui::TextButton(m1))
                        {
                            auto file = FileDialog::OpenFile({ { "hray", "hray" } });
                            OpenProject(file);
                        }
                    }
                    ImGui::End();
                }

                if (enableCreateNewProjectPopub)
                {
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
                            enableCreateNewProjectPopub = false;

                        {
                            ImGui::ScopedStyle swp(ImGuiStyleVar_WindowPadding, ImVec2(8, 8) * scale);

                            ImGui::BeginChild("Creat New Project", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);

                            bool validPath = std::filesystem::exists(projectPath);
                            bool validName = !std::filesystem::exists(std::filesystem::path(projectPath) / projectName);

                            {
                                ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

                                {
                                    ImGui::ScopedFont sf(FontType::Blod);
                                    ImGui::TextUnformatted("Project Name");
                                }
                                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

                                if (!validName) ImGui::PushStyleColor(ImGuiCol_Text, colors[Color::Error]);
                                ImGui::InputTextWithHint("##Project Name", "Project Name", &projectName);
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

                                if (!validPath) ImGui::PushStyleColor(ImGuiCol_Text, colors[Color::Error]);
                                ImGui::InputTextWithHint("##Location", "Location", &projectPath);
                                if (!validPath) ImGui::PopStyleColor();

                                ImGui::SameLine();
                                if (ImGui::Button(Icon_Folder, { -1,0 }))
                                {
                                    auto path = FileDialog::SelectFolder().string();
                                    if (!path.empty())
                                        projectPath = path;
                                }
                            }

                            ImGui::Dummy({ -1, 10 });

                            {
                                ImGui::ScopedFont sf(FontType::Blod);
                                ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

                                {
                                    ImGui::ScopedDisabled sd(!validName || !validPath);
                                    ImGui::ScopedButtonColor sbs(colors[Color::PrimaryButton]);
                                    if (ImGui::Button("Create Project", { -1,0 }))
                                    {
                                        CreateNewProject(projectPath, projectName);
                                        enableCreateNewProjectPopub = false;
                                    }
                                }

                                if (ImGui::Button("Cancel", { -1,0 }))
                                {
                                    enableCreateNewProjectPopub = false;
                                }
                            }

                            ImGui::EndChild();
                        }

                        ImGui::End();
                    }
                    ImGui::End();
                }
            }
            else
            {
                if (enableViewPortWindow)
                {
                    ImGui::ScopedStyle ss(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f,0.0f });
                    if (ImGui::Begin("View", &enableViewPortWindow))
                    {
                        auto size = ImGui::GetContentRegionAvail();
                        uint32_t w = AlignUp(uint32_t(size.x), 2u);
                        uint32_t h = AlignUp(uint32_t(size.y), 2u);

                        auto viewportMinRegion = ImGui::GetCursorScreenPos();
                        auto viewportMaxRegion = ImGui::GetCursorScreenPos() + ImGui::GetContentRegionAvail();
                        auto viewportOffset = ImGui::GetWindowPos();

                        if (useViewportSize && (width != w || height != h))
                        {
                            width = w;
                            height = h;
                        }

                        editorCamera->SetViewportSize(width, height);
                        editorCamera->OnUpdate(info.ts);

                        if (rd.renderTarget)
                        {
                            ImGui::Image(rd.renderTarget.Get(), ImVec2{ (float)size.x, (float)size.y });
                        }

                        // transform Gizmo
                        {
                            viewportBounds[0] = { viewportMinRegion.x , viewportMinRegion.y };
                            viewportBounds[1] = { viewportMaxRegion.x , viewportMaxRegion.y };

                            if (selectedEntity && gizmoType != -1)
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

                                auto& tc = selectedEntity.GetComponent<Assets::TransformComponent>();
                                Math::float4x4 entityWorldSpaceTransform = selectedEntity.GetWorldSpaceTransformMatrix();

                                bool snap = Input::IsKeyDown(Key::LeftControl);
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
                                    Math::mat4 parentWorldTransform = selectedEntity.GetParent().GetWorldSpaceTransformMatrix();
                                    Math::mat4 entityLocalSpaceTransform = Math::inverse(parentWorldTransform) * entityWorldSpaceTransform;

                                    Math::vec3 position, scale, skew;
                                    Math::quat quaternion;
                                    Math::vec4 perspective;
                                    Math::decompose(entityLocalSpaceTransform, scale, quaternion, position, skew, perspective);

                                    tc.position = position;
                                    tc.rotation = quaternion;
                                    tc.scale = scale;
                                }
                            }
                        }

                        // Toolbar
                        {
                            ImGui::ScopedFont sf(FontType::Blod, FontSize::BodyMedium);

                            static int location = 0;
                            Editor::BeginChildView("Toolbar", location);


                            ImVec2 buttonSize = ImVec2(30, 30) * io.FontGlobalScale + style.FramePadding * 2.0f;

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

                            Editor::EndChildView(location);
                        }

                        // Axis Gizmo
                        {
                            static int location = 1;
                            Editor::BeginChildView("Axis Gizmo", location);

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

                            Editor::EndChildView(location);
                        }

                        // Stats
                        {
                            static int location = 2;
                            Editor::BeginChildView("Stats", location);

                            const auto& appStats = Application::GetStats();
                            ImGui::Text("CPUMain %.2f ms | FPS %i", appStats.CPUMainTime, appStats.FPS);

                            Editor::EndChildView(location);
                        }
                    }
                    ImGui::End();
                }

                ImGui::ScopedStyle ss(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));

                Hierarchy();
                Inspector();
                if (enableAdvancedMode) AssetManager();
                if (enableAdvancedMode) Application();
            }
        }
    }

    void OnBegin(const FrameInfo& info) override
    {
        HE_PROFILE_FUNCTION();

        commandList->open();
        nvrhi::utils::ClearColorAttachment(commandList, info.fb, 0, nvrhi::Color(0.1f));
    }

    void OnEnd(const FrameInfo& info) override
    {
        HE_PROFILE_FUNCTION();

        commandList->close();
        device->executeCommandList(commandList);
    }

    void Inspector()
    {
        HE_PROFILE_FUNCTION();

        if (!enableInspectorWindow)
            return;

        ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(1, 1));
        ImGui::ScopedStyle is(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        if (ImGui::Begin(Icon_InfoCircle"  Inspector", &enableInspectorWindow, ImGuiWindowFlags_NoCollapse))
        {
            if (selectedEntity)
            {
                auto cf = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

                ImGui::ScopedColorStack sc(ImGuiCol_Header, ImVec4(0, 0, 0, 0), ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0), ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));

                auto& tc = selectedEntity.GetComponent<Assets::TransformComponent>();

                {
                    ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                    ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
                    ImGui::BeginChild("Name Component", { 0,0 }, cf);
                    auto& nc = selectedEntity.GetComponent<Assets::NameComponent>();
                    const float c_FLT_MIN = 1.175494351e-38F;
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputText("##Name", &nc.name, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::EndChild();
                }

                ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
                ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

                {
                    if (ImField::BeginBlock("Transform Component", Icon_Arros, transformColor))
                    {
                        if (ImGui::BeginTable("Transform Component", 2, ImGuiTableFlags_SizingFixedFit))
                        {
                            auto& tc = selectedEntity.GetComponent<Assets::TransformComponent>();

                            ImFieldDrageScalerEvent p = ImField::DragColoredFloat3("Position", &tc.position.x, 0.01f);
                            switch (p)
                            {
                            case ImFieldDrageScalerEvent_None:   break;
                            case ImFieldDrageScalerEvent_Edited: break;
                            case ImFieldDrageScalerEvent_ResetX: tc.position.x = 0; break;
                            case ImFieldDrageScalerEvent_ResetY: tc.position.y = 0; break;
                            case ImFieldDrageScalerEvent_ResetZ: tc.position.z = 0; break;
                            }

                            Math::float3 degrees = Math::degrees(tc.rotation.GetEuler());
                            ImFieldDrageScalerEvent r = ImField::DragColoredFloat3("Rotation", &degrees.x, 1.0f);
                            switch (r)
                            {
                            case ImFieldDrageScalerEvent_None:   break;
                            case ImFieldDrageScalerEvent_Edited: break;
                            case ImFieldDrageScalerEvent_ResetX: degrees.x = 0; break;
                            case ImFieldDrageScalerEvent_ResetY: degrees.y = 0; break;
                            case ImFieldDrageScalerEvent_ResetZ: degrees.z = 0; break;
                            }
                            tc.rotation = Math::radians(degrees);

                            ImFieldDrageScalerEvent s = ImField::DragColoredFloat3("Scale", &tc.scale.x, 0.01f);
                            switch (s)
                            {
                            case ImFieldDrageScalerEvent_None:   break;
                            case ImFieldDrageScalerEvent_Edited: break;
                            case ImFieldDrageScalerEvent_ResetX: tc.scale.x = 0; break;
                            case ImFieldDrageScalerEvent_ResetY: tc.scale.y = 0; break;
                            case ImFieldDrageScalerEvent_ResetZ: tc.scale.z = 0; break;
                            }

                            ImGui::EndTable();
                        }
                    }
                    ImField::EndBlock();
                }

                if (selectedEntity.HasComponent<Assets::MeshComponent>())
                {
                    if (ImField::BeginBlock("Mesh Component", Icon_Mesh, meshColor))
                    {
                        auto& dm = selectedEntity.GetComponent<Assets::MeshComponent>();
                        auto ms = assetManager.GetAsset<Assets::MeshSource>(dm.meshSourceHandle);
                        auto& meta = assetManager.GetMetadata(dm.meshSourceHandle);

                        if (ImGui::BeginTable("Mesh Component", 2, ImGuiTableFlags_SizingFixedFit))
                        {
                            if (ms)
                            {
                                if (dm.meshIndex < ms->meshes.size())
                                {
                                    auto& mesh = ms->meshes[dm.meshIndex];

                                    ImField::Button("Mesh Source", meta.filePath.string().c_str(), { -1, 0 });
                                    ImField::Button("Mesh", mesh.name.c_str(), { -1, 0 });
                                    ImField::InputUInt("Index", &dm.meshIndex);
                                    dm.meshIndex = dm.meshIndex >= (uint32_t)ms->meshes.size() ? (uint32_t)ms->meshes.size() - 1 : dm.meshIndex;
                                }
                            }
                            else
                            {
                                auto& meta = assetManager.GetMetadata(dm.meshSourceHandle);

                                ImField::Field("Mesh Source");
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                                ImGui::Button(meta.filePath.string().c_str(), { -1, 0 });
                                ImGui::PopStyleColor();

                                ImField::InputUInt("Index", &dm.meshIndex);
                            }

                            ImGui::EndTable();
                        }
                    }
                    ImField::EndBlock();


                    if (ImField::BeginBlock("Material Component", Icon_Mesh, meshColor))
                    {
                        auto& dm = selectedEntity.GetComponent<Assets::MeshComponent>();
                        auto ms = assetManager.GetAsset<Assets::MeshSource>(dm.meshSourceHandle);
                        auto& mesh = ms->meshes[dm.meshIndex];
                        
                        //if (ImGui::BeginTable("Material Component", 2, ImGuiTableFlags_SizingFixedFit))
                        {
                            int i = 0;
                            for (auto& geo : mesh.GetGeometrySpan())
                            {
                                ImGui::ScopedID sid(i);

                                {
                                    auto mat = assetManager.GetAsset<Assets::Material>(geo.materailHandle);
                                    if (mat)
                                    {
                                        //ImField::Field("Bace Color");
                                        ImGui::ColorEdit4("Bace Color", &mat->baseColor.x);
                                        ImGui::ColorEdit4("Emissive Color", &mat->emissiveColor.x);
                                        ImGui::DragFloat("Emissive Exposure", &mat->emissiveEV);
                                    }

                                    //ImGui::TreePop();
                                }
                                i++;
                            }

                            //ImGui::EndTable();
                        }
                    }
                    ImField::EndBlock();
                }
            }
        }
        ImGui::End();
    }

    void DrawHierarchy(Assets::Entity parent, Assets::Scene* scene)
    {
        HE_PROFILE_FUNCTION();

        auto& children = parent.GetChildren();
        for (uint32_t i = 0; i < children.size(); i++)
        {
            auto childID = children[i];

            Assets::Entity childEntity = scene->FindEntity(childID);

            auto& nc = childEntity.GetComponent<Assets::NameComponent>();

            bool isSelected = selectedEntity == childEntity;

            ImGui::PushID((int)childID);

            ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow;
            node_flags = childEntity.GetChildren().empty() ? node_flags | ImGuiTreeNodeFlags_Leaf : node_flags;
            node_flags = isSelected ? node_flags | ImGuiTreeNodeFlags_Selected : node_flags;

            bool open = ImGui::TreeNodeEx(nc.name.c_str(), node_flags, "%s  %s", Icon_DICE, nc.name.c_str());
            if (ImGui::IsItemClicked())
                selectedEntity = childEntity;

            if (open)
            {
                if (childEntity)
                    DrawHierarchy(childEntity, scene);
                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    void Hierarchy()
    {
        HE_PROFILE_FUNCTION();

        if (!enableHierarchyWindow)
            return;

        static ImGuiTextFilter filter;

        ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(1, 1));
        ImGui::ScopedStyle is(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        if (ImGui::Begin(Icon_Hierarchy"  Hierarchy", &enableHierarchyWindow, ImGuiWindowFlags_NoScrollbar))
        {
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
                Assets::Scene* scene = assetManager.GetAsset<Assets::Scene>(sceneHandle);
                if (scene) DrawHierarchy(scene->GetRootEntity(), scene);
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    void DrawAssetItem(Assets::AssetHandle handle, Assets::AssetMetadata& metaData)
    {
        HE_PROFILE_FUNCTION();

        auto am = &assetManager;

        ImGui::PushID((int)handle);

        bool loaded = am->IsAssetLoaded(handle);

        if (loaded) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.3f, 1.0f));

        {
            ImVec2 size = ImGui::CalcTextSize("Handle") + ImVec2(20, 0);
            bool fileExists = std::filesystem::exists(am->GetAssetFileSystemPath(handle));
            bool validHandle = am->IsAssetHandleValid(handle);

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
                        am->metaMap[handle].type = t;
                        am->Serialize();
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
                        am->UpdateMetadate(handle, metaData);
                    }
                    if (!fileExists) ImGui::PopStyleColor();
                }
                ImGui::EndTable();
            }

            {
                float width = ImGui::GetContentRegionAvail().x / 4 - ImGui::GetCurrentContext()->CurrentWindow->WindowPadding.x;

                if (ImGui::Button("Remove", ImVec2(width, 0)))
                {
                    am->RemoveAsset(handle);
                }
                ImGui::SameLine();

                ImGui::BeginDisabled(!am->IsAssetLoaded(handle));
                if (ImGui::Button("Unload", ImVec2(width, 0)))
                {
                    am->UnloadAsset(handle);
                }

                ImGui::SameLine();

                if (ImGui::Button("Reload", ImVec2(width, 0)) && fileExists)
                {
                    am->ReloadAsset(handle);
                }
                ImGui::EndDisabled();

                ImGui::SameLine();

                if (ImGui::Button("Open", ImVec2(-1, 0)) && fileExists)
                {
                    FileSystem::Open(am->GetAssetFileSystemPath(handle));
                }
            }
        }
        if (loaded) ImGui::PopStyleColor();

        ImGui::Dummy({ -1,4 });
        ImGui::Separator();
        ImGui::Dummy({ -1,4 });

        ImGui::PopID();
    }

    void AssetManager()
    {
        HE_PROFILE_FUNCTION();

        if (!enableAssetManagerWindow)
            return;

        ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(1, 1));
        ImGui::ScopedStyle is(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

        if (ImGui::Begin("AssetManager", &enableAssetManagerWindow))
        {
            static  ImGuiTextFilter filter;

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
                        for (auto& [handle, metaData] : assetManager.metaMap)
                        {
                            std::string pathStr = metaData.filePath.string();
                            std::string handleStr = std::to_string(handle);
                            if (!filter.PassFilter(handleStr.c_str()) && !filter.PassFilter(pathStr.c_str()) && !filter.PassFilter(magic_enum::enum_name<Assets::AssetType>(metaData.type).data()))
                                continue;

                            DrawAssetItem(handle, metaData);
                        }
                    }
                    else
                    {
                        ImGuiListClipper clipper;
                        clipper.Begin(static_cast<int>(assetManager.metaMap.size()));
                        while (clipper.Step())
                        {
                            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                            {
                                auto& [handle, metaData] = *(std::next(assetManager.metaMap.begin(), i));

                                DrawAssetItem(handle, metaData);
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
                            auto currentTypeStr = magic_enum::enum_name<Assets::AssetImportingMode>(assetManager.desc.importMode);
                            auto types = magic_enum::enum_names<Assets::AssetImportingMode>();
                            int selected = 0;
                            if (ImField::Combo("Import Mode", types, currentTypeStr, selected))
                            {
                                auto t = magic_enum::enum_cast<Assets::AssetImportingMode>(types[selected]).value();
                                assetManager.desc.importMode = t;
                            }

                            ImField::TextLinkOpenURL("Asset Registry", assetManager.desc.assetsRegistryFilePath.string().c_str());
                            ImField::Text("Registered Assets", "%zd", assetManager.metaMap.size());
                            ImField::Text("Subscribers", "%zd", assetManager.subscribers.size());
                            ImField::Text("Async Task Count", "%zd", assetManager.asyncTaskCount);

                            ImGui::EndTable();
                        }
                    }
                    ImField::EndBlock();


                    if (ImField::BeginBlock("Loaded Assets"))
                    {
                        if (ImGui::BeginTable("Loaded Assets", 2, ImGuiTableFlags_SizingFixedFit))
                        {
                            ImField::Text("Textures", "%zd", assetManager.registry.view<Assets::Texture>().size());
                            ImField::Text("Materials", "%zd", assetManager.registry.view<Assets::Material>().size());
                            ImField::Text("MeshSources", "%zd", assetManager.registry.view<Assets::MeshSource>().size());
                            ImField::Text("Scenes", "%zd", assetManager.registry.view<Assets::Scene>().size());

                            ImGui::EndTable();
                        }
                    }
                    ImField::EndBlock();

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void Application()
    {
        HE_PROFILE_FUNCTION();

        if (!enableApplicationWindow)
            return;

        if (ImGui::Begin("Application", &enableApplicationWindow))
        {
            auto cf = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

            ImGui::ScopedColor sc0(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
            ImGui::ScopedColor sc1(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
            ImGui::ScopedColor sc2(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));

            if (ImField::BeginBlock("Settings"))
            {
                if (ImGui::BeginTable("Scene", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    ImField::Checkbox("VSync", &Application::GetWindow().swapChain->desc.vsync);

                    ImGui::EndTable();
                }
            }
            ImField::EndBlock();

            if (ImField::BeginBlock("Stats"))
            {
                if (ImGui::BeginTable("Scene", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    ImField::Text("Geometry Count", "%zd", rd.geometryCount);
                    ImField::Text("Instance Count", "%zd", rd.instanceCount);
                    ImField::Text("Material Count", "%zd", rd.materialCount);
                    ImField::Text("Texture Count", "%zd", rd.textureCount);

                    Assets::Scene* scene = assetManager.GetAsset<Assets::Scene>(sceneHandle);
                    if (scene)
                    {
                        auto transformComponentCount = scene->registry.view<Assets::TransformComponent>().size();
                        auto meshComponentCount = scene->registry.view<Assets::MeshComponent>().size();
                        ImField::Text("transform Component Count", "%zd", transformComponentCount);
                        ImField::Text("mesh Component Count", "%zd", meshComponentCount);
                    }

                    ImGui::EndTable();
                }
            }
            ImField::EndBlock();
        }

        ImGui::End();
    }

    void SetFlyCamera()
    {
        HE_PROFILE_FUNCTION();

        flyCamera->transform = orbitCamera->transform;

        editorCamera = flyCamera;
        editorCamera->UpdateView();
        editorCamera->SetViewportSize(width, height);

        Serialize();
    }

    void SetOrbitCamera()
    {
        HE_PROFILE_FUNCTION();

        orbitCamera->transform = flyCamera->transform;

        Math::vec3 focalPoint = flyCamera->transform.position - flyCamera->transform.GetForward() * 10.0f;
        orbitCamera->focalPoint = focalPoint;
        orbitCamera->distance = 10;

        editorCamera = orbitCamera;
        editorCamera->UpdateView();
        editorCamera->SetViewportSize(width, height);

        Serialize();
    }

    void FocusCamera()
    {
        if (selectedEntity)
        {
            auto wt = selectedEntity.GetWorldSpaceTransformMatrix();
            Math::vec3 p, s, skew;
            Math::quat quaternion;
            Math::vec4 perspective;
            Math::decompose(wt, s, quaternion, p, skew, perspective);

            Math::box3 aabb({ -1.0f,-1.0f,-1.0f }, { 1.0f ,1.0f ,1.0f });
            if (selectedEntity.HasComponent<Assets::MeshComponent>())
            {
                auto& mc = selectedEntity.GetComponent<Assets::MeshComponent>();
                auto meshSource = assetManager.GetAsset<Assets::MeshSource>(mc.meshSourceHandle);
                auto& mesh = meshSource->meshes[mc.meshIndex];
                aabb = mesh.aabb;
            }

            aabb = Math::ConvertBoxToWorldSpace(wt, aabb);
            auto length = Math::length(aabb.diagonal());

            editorCamera->Focus(aabb.center(), length);
        }
    }

    void ImportModel(const std::filesystem::path& file)
    {
        HE_PROFILE_FUNCTION();
       
        if (std::filesystem::exists(file))
        {
            importing = true;
            auto handle = assetManager.GetOrMakeAsset(file, "Meshes" / file.filename());
            auto meshSource = assetManager.GetAsset<Assets::MeshSource>(handle);
        }
    }

    void OpenProject(const std::filesystem::path& file)
    {
        selectedEntity = {};

        if (std::filesystem::exists(file))
        {
            auto cacheDir = file.parent_path() / "Cache";

            project.projectFilePath = file;
            project.assetsDir = file.parent_path() / "Assets";
            project.assetsMetaDataFilePath = cacheDir / "assetsMetaData.json";

            assetManager.Reset();
            assetManager.desc.importMode = Assets::AssetImportingMode::Async;
            assetManager.desc.assetsDirectory = project.assetsDir;
            assetManager.desc.assetsRegistryFilePath = project.assetsMetaDataFilePath;
            AssetEventCallbackHandle = assetManager.Subscribe(this);
            assetManager.Deserialize();
            Deserialize();

            enableStartMenu = false;
        }
    }

    void CreateNewProject(const std::filesystem::path& path, std::string projectName)
    {
        selectedEntity = {};
        auto newProjectDir = path / projectName;
        auto cacheDir = newProjectDir / "Cache";
        project.projectFilePath = newProjectDir / std::format("{}.hray", projectName);
        project.assetsDir = newProjectDir / "Assets";
        project.assetsMetaDataFilePath = cacheDir / "assetsMetaData.json";
        enableStartMenu = false;

        std::filesystem::create_directories(cacheDir);
        std::filesystem::create_directories(project.assetsDir);

        assetManager.Reset();
        assetManager.desc.importMode = Assets::AssetImportingMode::Async;
        assetManager.desc.assetsDirectory = project.assetsDir;
        assetManager.desc.assetsRegistryFilePath = project.assetsMetaDataFilePath;
        AssetEventCallbackHandle = assetManager.Subscribe(this);
        Serialize();
    }

    void OpenScene()
    {
        HE_PROFILE_FUNCTION();

        auto file = FileDialog::OpenFile({ { "scene", "scene" } });

        if (std::filesystem::exists(file))
        {
            Jops::SubmitToMainThread([this, file]() {

                auto assetFile = std::filesystem::relative(file, assetManager.desc.assetsDirectory);
                sceneHandle = assetManager.GetAssetHandleFromFilePath(assetFile);
                if (!assetManager.IsAssetHandleValid(sceneHandle))
                    sceneHandle = assetManager.GetOrMakeAsset(file, assetFile);

                auto scene = assetManager.GetAsset<Assets::Scene>(sceneHandle);
                HE_ASSERT(scene);
               
                Serialize();
            });
        }
    }

    void NewScene()
    {
        HE_PROFILE_FUNCTION();

        auto file = FileDialog::SaveFile({ { "scene", "scene" } });
        if (!file.empty())
        {
            assetManager.UnloadAllAssets();

            auto assetFile = std::filesystem::relative(file, assetManager.desc.assetsDirectory);
            sceneHandle = assetManager.CreateAsset(assetFile).GetHandle();
            Serialize();
        }
    }

    void Serialize()
    {
        HE_PROFILE_FUNCTION();

        std::ofstream file(project.projectFilePath);
        std::ostringstream out;
        out << "{\n";

        // scene
        {
            out << "\t\"sceneHandle\" : " << sceneHandle << ",\n";
        }

        // cameras
        {
            out << "\t\"cameras\" : {\n";
            {
                out << "\t\t\"position\" : " << editorCamera->transform.position << ",\n";
                out << "\t\t\"rotation\" : " << Math::eulerAngles(editorCamera->transform.rotation) << ",\n";
                out << "\t\t\"cameraType\" : " << (std::dynamic_pointer_cast<Editor::OrbitCamera>(editorCamera) ? 0 : 1) << ",\n";

                out << "\t\t\"orbitCamera\" : {\n";
                {
                    out << "\t\t\t\"focalPoint\" : " << orbitCamera->focalPoint << ",\n";
                    out << "\t\t\t\"distance\" : " << orbitCamera->distance << "\n";
                }
                out << "\t\t},\n";

                out << "\t\t\"flyCamera\" : {\n";
                {
                    out << "\t\t\t\"speed\" : " << flyCamera->speed << "\n";
                }
                out << "\t\t}\n";
            }
            out << "\t},\n";
        }
        
        // ui
        {
            out << "\t\"ui\" : {\n";
            {
                
                //out << "\t\t\"fontScale\" : " << ImGui::GetIO().FontGlobalScale << ",\n";
                out << "\t\t\"enableTitlebar\" : " << (enableTitlebar ? "true" : "false") << ",\n";
                out << "\t\t\"enableViewPortWindow\" : " << (enableViewPortWindow ? "true" : "false") << ",\n";
                out << "\t\t\"enableHierarchyWindow\" : " << (enableHierarchyWindow ? "true" : "false") << ",\n";
                out << "\t\t\"enableInspectorWindow\" : " << (enableInspectorWindow ? "true" : "false") << ",\n";
                out << "\t\t\"enableAssetManagerWindow\" : " << (enableAssetManagerWindow ? "true" : "false") << ",\n";
                out << "\t\t\"enableApplicationWindow\" : " << (enableApplicationWindow ? "true" : "false") << ",\n";
                out << "\t\t\"enableAdvancedMode\" : " << (enableAdvancedMode ? "true" : "false") << "\n";
            }
            out << "\t}\n";
        }

        out << "}\n";

        file << out.str();
        file.close();

        if (ImGui::GetCurrentContext())
        {
            std::string layoutPath = (appData / "layout.ini").lexically_normal().string();
            ImGui::GetIO().IniFilename = nullptr;
            ImGui::SaveIniSettingsToDisk(layoutPath.c_str());
        }
    }

    bool Deserialize()
    {
        HE_PROFILE_FUNCTION();

        if (!std::filesystem::exists(project.projectFilePath))
        {
            HE_ERROR("HRAy::Deserialize : Unable to open file for reaading, {}", project.projectFilePath.string());
            return false;
        }

        static simdjson::dom::parser parser;
        auto doc = parser.load(project.projectFilePath.string());

        // scene
        {
            auto sceneHandleData = doc["sceneHandle"];
            if (!sceneHandleData.error())
            {
                sceneHandle = sceneHandleData.get_uint64().value();
            }
        }

        auto cameras = doc["cameras"];
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
                orbitCamera->focalPoint = {
                    (float)orbitCameraData["focalPoint"].get_array().at(0).get_double().value(),
                    (float)orbitCameraData["focalPoint"].get_array().at(1).get_double().value(),
                    (float)orbitCameraData["focalPoint"].get_array().at(2).get_double().value()
                };
                orbitCamera->distance = (float)orbitCameraData["distance"].get_double().value();
            }

            auto flyCameraData = cameras["flyCamera"];
            if (!flyCameraData.error())
            {
                flyCamera->speed = (float)flyCameraData["speed"].get_double().value();
            }
        }

        auto ui = doc["ui"];
        if (!ui.error())
        {
            {
                //auto val = ui["fontScale"];
                //if (!val.error())  ImGui::GetIO().FontGlobalScale = (float)val.get_double().value();
            }

            {
                auto enable = ui["enableTitlebar"];
                if (!enable.error()) enableTitlebar = enable.get_bool().value();
            }

            {
                auto enable = ui["enableViewPortWindow"];
                if (!enable.error()) enableViewPortWindow = enable.get_bool().value();
            }

            {
                auto enable = ui["enableHierarchyWindow"];
                if (!enable.error()) enableHierarchyWindow = enable.get_bool().value();
            }

            {
                auto enable = ui["enableInspectorWindow"];
                if (!enable.error()) enableInspectorWindow = enable.get_bool().value();
            }

            {
                auto enable = ui["enableAssetManagerWindow"];
                if (!enable.error()) enableAssetManagerWindow = enable.get_bool().value();
            }

            {
                auto enable = ui["enableApplicationWindow"];
                if (!enable.error()) enableApplicationWindow = enable.get_bool().value();
            }

            {
                auto enable = ui["enableAdvancedMode"];
                if (!enable.error()) enableAdvancedMode = enable.get_bool().value();
            }
        }

        return true;
    }
};

HE::ApplicationContext* HE::CreateApplication(ApplicationCommandLineArgs args)
{
    ApplicationDesc desc;
    desc.commandLineArgs = args;

#ifdef HE_DIST
    if (args.count == 2)
        desc.workingDirectory = std::filesystem::path(args[0]).parent_path();
#endif

    //desc.deviceDesc.enableGPUValidation = true;
    //desc.deviceDesc.enableDebugRuntime = true;
    //desc.deviceDesc.enableNvrhiValidationLayer = true;

    desc.deviceDesc.enableRayTracingExtensions = true;
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

    auto log = (FileSystem::GetAppDataPath(desc.windowDesc.title) / desc.windowDesc.title).string();
    desc.logFile = log.c_str();

    ApplicationContext* ctx = new ApplicationContext(desc);
    Application::PushLayer(new HRayApp());

    return ctx;
}

#include "HydraEngine/EntryPoint.h"
