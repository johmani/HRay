module;

#include "HydraEngine/Base.h"

export module Editor;

import HE;
import std;
import Math;
import ImGui;
import Assets;
import HRay;
import nvrhi;
import simdjson;
import Tiny2D;

export namespace Editor {

    //////////////////////////////////////////////////////////////////////////
    // Window
    //////////////////////////////////////////////////////////////////////////

    using WindowHandle = uint64_t;

    struct WindowDesc
    {
        std::string title;
        std::string Icon;
        std::string invokePath;
        std::string tooltip;
        bool menuBar = false;
        ImVec2 padding = { 1, 1 };
        int maxWindowInstances = 1;
        
        // internal
        ImVec2 size;
        bool focusRequst;
        int instanceIndex = 0;
    };

    struct Window
    {
        virtual ~Window() {}
        virtual void OnCreate() {}
        virtual void OnBegin(HE::Timestep ts) {}
        virtual void OnUpdate(HE::Timestep ts) {}
        virtual void OnEnd(HE::Timestep ts) {}
        virtual void OnDestroy() {}
        virtual void OnResize(uint32_t width, uint32_t height) {}
    };

    struct SceneCallback
    {
        virtual void OnAnimationStart() {}
        virtual void OnAnimationStop() {}
    };

    struct SerializationCallback
    {
        virtual void Serialize(std::ostringstream& out) {}
        virtual void Deserialize(simdjson::dom::element element) {}
    };

    struct WindowScript
    {
        Window* instance = nullptr;

        std::function<Window* ()> InstantiateScript;
        void (*DestroyScript)(WindowScript*);

        template<typename T>
        T* As() { return (T*)(instance); }

        WindowScript() = default;
        ~WindowScript() = default;

        template<typename T, typename... Args>
        void Bind(Args&&... args)
        {
            InstantiateScript = [args...]() -> Window* { return new T(args...); };
            DestroyScript     = [](WindowScript* nsc) {  delete nsc->instance; nsc->instance = nullptr; };
        }
    };

    enum class WindowsLayout
    {
        Default
    };

    struct WindowManager
    {
        std::vector<WindowScript> scripts;
        std::vector<WindowDesc> descs;
        std::map<std::string, bool> state;

        WindowManager();
        ~WindowManager();
    };

    void DistroyWindow(WindowHandle handle);
    void UpdateMenuItems();
    void UpdateWindows(HE::Timestep ts);
    void OpenWindow(const char* windowName);
    void CloseWindow(std::string_view windowName);
    void FocusWindow(std::string_view windowName);
    void CloseAllWindows();
    void LoadWindowsLayout(WindowsLayout layout = WindowsLayout::Default);
    WindowDesc& GetWindowDesc(WindowHandle handle);
    WindowDesc& GetWindowDesc(Window* window);
    void SerializeWindows(std::ostringstream& out);
    void DeserializeWindows(simdjson::dom::element element);
    void DeserializeWindow(WindowHandle handle);
    void SerializeWindowsState();
    void DeserializeWindowsState();

    //////////////////////////////////////////////////////////////////////////
    // Camera
    //////////////////////////////////////////////////////////////////////////

    struct CameraTransform
    {
        Math::float3 position = { 0.0f, 0.0f, 0.0f };
        Math::quat rotation = { 1.0f, 0.0f, 0.0f, 0.0f }; //Math::quat(Math::radians(Math::float3(-45.0f, -45.0f, 0.0f)));

        Math::float3 GetRight() { return Math::rotate(rotation, Math::float3(1.0f, 0.0f, 0.0f)); }
        Math::float3 GetUp() { return Math::rotate(rotation, Math::float3(0.0f, 1.0f, 0.0f)); }
        Math::float3 GetForward() { return Math::rotate(rotation, Math::float3(0.0f, 0.0f, 1.0f)); }
        Math::float4x4 ToMat() const { return Math::translate(Math::float4x4(1.0f), position) * Math::toMat4(Math::quat(rotation)); }
    };

    struct View
    {
        Math::float4x4 projection;
        Math::float4x4 viewProjection;
        Math::float4x4 view;
        float fov;
        float near;
        float far;
    };

    struct EditorCamera
    {
        CameraTransform transform;
        View view;

        virtual void OnUpdate(HE::Timestep ts) {}
        virtual void SetViewportSize(uint32_t width, uint32_t height) {}
        virtual void Focus(Math::float3 position, float distance) {}
        virtual void UpdateView() {}
    };

    struct OrbitCamera : public EditorCamera
    {
        float rotationSpeed = 4.0f;
        float distance = 5.0f;
        Math::float3 focalPoint = { 0.0f, 0.0f, 0.0f };
        Math::float2 initialMousePosition = { 0.0f, 0.0f };
        bool overrideTransform = false;

        OrbitCamera() = default;
        OrbitCamera(float fov, float aspectRatio, float near, float far);
        float ZoomSpeed(float distance);
        void OnUpdate(HE::Timestep ts) override;
        virtual void SetViewportSize(uint32_t width, uint32_t height) override;
        virtual void Focus(Math::float3 position, float pDistance) override;
        virtual void UpdateView() override;
    };

    struct FlyCamera : public EditorCamera
    {
        float speed = 10.0f;
        Math::float2 initialMousePosition = { 0.0f, 0.0f };

        FlyCamera() = default;
        FlyCamera(float fov, float aspectRatio, float near, float far, float speed = 10.0f);
        void OnUpdate(HE::Timestep ts) override;
        virtual void Focus(Math::float3 position, float distance) override;
        virtual void SetViewportSize(uint32_t width, uint32_t height) override;
        virtual void UpdateView() override;
    };

    //////////////////////////////////////////////////////////////////////////
    // Style
    //////////////////////////////////////////////////////////////////////////

    struct Color
    {
        enum
        {
            PrimaryButton,
            Selected,
            ViewPortSelected,

            TextButtonHovered,
            TextButtonActive,

            Dangerous,
            DangerousHovered,
            DangerousActive,

            Info,
            Warn,
            Error,

            ChildBlock,

            Mesh,
            Transform,
            Light,
            Material,
            Camera,

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

    enum class AppIcons
    {
        AppIcon,
        Close,
        Minimize, 
        Maximize,
        Restore,
        Board,

        Camera,
        DirectionalLight,
        EnvLight,

        Count
    };

    //////////////////////////////////////////////////////////////////////////
    // App
    //////////////////////////////////////////////////////////////////////////

    enum class Primitive
    {
        Capsule,
        Cone,
        Cube,
        Cylinder,
        IcoSphere,
        Plane,
        Suzanne,
        Torus,
        UVSphere
    };

    struct Project
    {
        std::filesystem::path projectFilePath;
        std::filesystem::path assetsMetaDataFilePath;
        std::string layoutFilePath;
        std::filesystem::path cacheDir;
        std::filesystem::path assetsDir;
    };

    enum class SceneMode
    {
        Editor,
        Runtime,
    };

    struct Animation
    {
        enum State
        {
            None = BIT(0),
            Forward = BIT(1),
            Back = BIT(2),
            Animating = Forward | Back,
        } state;

        float t = 0.0f;
        float duration = 0.2f;

        Math::float3 startPosition;
        Math::quat   startRotation;

        Math::float3 endPosition;
        Math::quat   endRotation;
    };

    struct PixelReadbackPass
    {
        void Init(nvrhi::IDevice* device);
        ~PixelReadbackPass();
        void Capture(nvrhi::ICommandList* commandList, nvrhi::ITexture* inputTexture, Math::uvec2 pixelPosition);
        uint32_t ReadUInt();

        nvrhi::DeviceHandle device;
        nvrhi::ShaderHandle cs;
        nvrhi::ComputePipelineHandle pipeline;
        nvrhi::BindingLayoutHandle bindingLayout;
        nvrhi::BindingSetHandle bindingSet;
        nvrhi::BufferHandle constantBuffer;
        nvrhi::BufferHandle intermediateBuffer;
        nvrhi::BufferHandle readbackBuffer;
        void* mapedBuffer = nullptr;
    };

    struct App;
    struct Context
    {
        App* app;
        nvrhi::DeviceHandle device;
        nvrhi::CommandListHandle commandList;
        
        std::array<nvrhi::TextureHandle, (int)AppIcons::Count> icons;

        Assets::AssetManager assetManager;
        Assets::SubscriberHandle assetEventCallbackHandle = 0;
        
        Assets::AssetHandle sceneHandle = 0;
        Assets::AssetHandle tempSceneHandle;

        SceneMode sceneMode = SceneMode::Editor;

        HRay::RendererData rd;
        HRay::FrameData fd;

        WindowManager windowManager;

        std::unordered_map<std::string, std::function<Assets::Entity(Assets::Scene*, Assets::UUID)>> createEnityFucntions;

        ImGuiTextFilter textFilter;

        ImVec4 colors[Color::Count];
        float fontScale = 1.0f;

        Assets::Entity selectedEntity;

        bool enableTitlebar = true;
        bool enableStartMenu = true;
        bool enableCreateNewProjectPopub = false;
        bool importing = false;

        std::string outputPath;
        std::string projectPath;
        std::string projectName;
        std::filesystem::path appData;
        std::filesystem::path keyBindingsFilePath;
        Project project;

        int width = 1920;
        int height = 1080;

        int frameStart = 0;
        int frameEnd = 50;
        int frameStep = 1;
        int maxSamples = 1024;
        
        int sampleCount = 0;
        int frameIndex = 0;
    };
   
    struct App : public HE::Layer, public Assets::AssetEventCallback
    {
        inline static Context* s_Context;

        void OnAttach() override;
        void OnDetach() override;
        void OnEvent(HE::Event& e) override;
        bool OnWindowDropEvent(HE::WindowDropEvent& e);
        void OnAssetLoaded(Assets::Asset asset) override;
        void OnAssetUnloaded(Assets::Asset asset) override;
        void OnUpdate(const HE::FrameInfo& info) override;
        void OnBegin(const HE::FrameInfo& info) override;
        void OnEnd(const HE::FrameInfo& info) override;
    };

    Context& GetContext();
    Assets::AssetManager& GetAssetManager();

    ImVec4 GetColor(int c);
    nvrhi::ITexture* GetIcon(AppIcons icon);

    Assets::Entity GetSceneCamera(Assets::Scene* scene);
   
    void SetRendererToSceneCameraProp(HRay::FrameData& frameData, const Assets::CameraComponent& c);

    void Animate();
    void Stop();

    void ImportModel(const std::filesystem::path& file);
    void OpenProject(const std::filesystem::path& file);
    void CreateNewProject(const std::filesystem::path& path, std::string projectName);
    void OpenScene();
    void NewScene();

    void OpenStartMeue();

    void Clear();

    Assets::Entity GetSelectedEntity();
    void SelectEntity(Assets::Entity entity);
    Assets::Scene* GetScene();

    void Save(nvrhi::IDevice* device, nvrhi::ITexture* texture, const std::string& directory, uint32_t frameIndex);
    void ImportMeshSource(Assets::Scene* scene, Assets::Entity parent, Assets::Node& node, Assets::Asset asset)
    {
        for (auto& node : node.GetChildren(asset.Get<Assets::MeshSourecHierarchy>()))
        {
            Assets::Entity newEntity = scene->CreateEntity(node.name, parent.GetUUID());
    
            if (node.index != Assets::c_Invalid)
            {
                switch (node.type)
                {
                case Assets::NodeType::Mesh:
                {
                    auto& meshSource = asset.Get<Assets::MeshSource>();
                    auto& mesh = meshSource.meshes[node.index];

                    auto& dmc = newEntity.AddComponent<Assets::MeshComponent>();
                    dmc.meshSourceHandle = asset.GetHandle();
                    dmc.meshIndex = node.index;

                    break;
                }
                case Assets::NodeType::Camera:
                {
                    auto& meshSource = asset.Get<Assets::MeshSource>();
                    auto& camera = meshSource.cameras[node.index];

                    auto& cc = newEntity.AddComponent<Assets::CameraComponent>();
                    cc.perspectiveNear = camera.zNear;
                    cc.perspectiveFar = camera.zFar;
                    cc.perspectiveFieldOfView = camera.yfov;

                    break;
                }
                }
            }
            
    
            Math::float3 position, scale, skew;
            Math::quat quaternion;
            Math::float4 perspective;
            Math::decompose(node.transform, scale, quaternion, position, skew, perspective);
    
            auto& tc = newEntity.GetComponent<Assets::TransformComponent>();
            tc.position = position;
            tc.rotation = quaternion;
            tc.scale = scale;
    
            ImportMeshSource(scene, newEntity, node, asset);
        }
    }

    bool IsEntityNameExistInChildren(Assets::Scene* scene, const std::string& name, Assets::Entity entity);
    std::string GetIncrementedReletiveEntityName(Assets::Scene* scene, const std::string& name, Assets::Entity parent);

    Assets::Entity CreateEntity(Assets::Scene* scene, const std::string& key, Assets::UUID parent);
    void AddEntityFactory(const std::string& key, std::function<Assets::Entity(Assets::Scene* scene, Assets::UUID)> function);

    void Serialize();
    bool Deserialize();

    void OnUpdateFrame();

    template<typename T, typename... Args>
    WindowHandle BindWindow(const WindowDesc& desc, Args&&... args)
    {
        auto& windowManager = Editor::GetContext().windowManager;
       
        if (desc.maxWindowInstances > 1)
        {
            for (int i = 0; i < desc.maxWindowInstances; i++)
            {
                auto d = desc;
                if (i > 0)
                {
                    d.title = std::format("{} {}", desc.title, i);
                    d.invokePath = std::format("{}/{}", desc.invokePath, d.title);
                }
                else
                {
                    d.title = desc.title;
                    d.invokePath = std::format("{}/{}", desc.invokePath, d.title);
                }

                windowManager.descs.push_back(d);

                auto& script = windowManager.scripts.emplace_back();
                script.Bind<T>(std::forward<Args>(args)...);
            }
        }
        else
        {
            windowManager.descs.push_back(desc);
            auto& script = windowManager.scripts.emplace_back();
            script.Bind<T>(std::forward<Args>(args)...);
        }

        return 0;
    }

    //////////////////////////////////////////////////////////////////////////
    // Utils
    //////////////////////////////////////////////////////////////////////////

    enum class Corner
    {
        TopLeft,
        TopRight,
        ButtomLeft,
        ButtomRight
    };

    bool BeginMainMenuBar(bool customTitlebar, ImTextureRef icon, ImTextureRef close, ImTextureRef min, ImTextureRef max, ImTextureRef res);
    void EndMainMenuBar();
    void BeginChildView(const char* str_id, Corner& location, ImVec2 padding = 5);
    void EndChildView();
    bool AssetPicker(const char* name, Assets::AssetHandle& assetHandle, Assets::AssetType type, Assets::Texture* texture = nullptr, ImVec2 size = { 0, 0 });
    std::string IncrementString(const std::string& str);


    //////////////////////////////////////////////////////////////////////////
    // App Windows
    //////////////////////////////////////////////////////////////////////////

    struct ViewPortWindow : Window, SerializationCallback
    {
        struct Debug
        {
            bool enableMeshBitangents = false;
            bool enableMeshTangents = false;
            bool enableMeshNormals = false;
            bool enableMeshAABB = false;
            bool enableStats = false;
            float lineLength = 0.05f;
            float colorOpacity = 1.0f;
        } debug;

        bool openMeshDebuggingOverlay = false;
        bool openViewOverlay = false;
        
        HE::Ref<OrbitCamera> orbitCamera;
        HE::Ref<FlyCamera> flyCamera;
        HE::Ref<EditorCamera> editorCamera;
        int width = 1920, height = 1080;
        Math::vec2 viewportBounds[2];

        int gizmoType = ImGuizmo::TRANSLATE;
        
        Animation cameraAnimation;
        bool previewMode = false;

        Math::float3 cameraPrevPos;
        Math::quat cameraPrevRot;
        bool clearReq = false;

        Corner toolbarCorner = Corner::TopLeft;
        Corner axisGizmoCorner = Corner::TopRight;
        Corner statsCorner = Corner::ButtomLeft; 

        HRay::FrameData fd;
        Tiny2D::ViewHandle tiny2DView;

        nvrhi::BindingLayoutHandle compositeBindingLayout;
        nvrhi::BindingSetHandle compositeBindingSet;
        nvrhi::ComputePipelineHandle computePipeline;
        nvrhi::ShaderHandle cs;
        nvrhi::TextureHandle compositeTarget;
        nvrhi::TextureHandle idTarget;

        PixelReadbackPass pixelReadbackPass;
        Math::uvec2 pixelPosition = { 0, 0 };
        uint32_t selected = -1;

        Assets::Entity GetHoveredEntity(Assets::Scene* scene);

        void OnCreate() override;
        void OnUpdate(HE::Timestep ts) override;
        void OnEnd(HE::Timestep ts) override;
        void Serialize(std::ostringstream& out) override;
        void Deserialize(simdjson::dom::element element) override;

        void UpdateEditorCameraAnimation(Assets::Scene* scene, Assets::Entity mainCameraEntity, float ts);
        void Preview();
        void StopPreview(bool moveBack = true);
        void SetRendererToEditorCameraProp(HRay::FrameData& frameData);

        void SetFlyCamera();
        void SetOrbitCamera();
        void FocusCamera();
        void AlignActiveCameraToView(Assets::Scene* scene);
    };

    struct OutputWindow : Window
    {
        Corner statsCorner = Corner::ButtomLeft;

        void OnUpdate(HE::Timestep ts) override;
    };

    struct HierarchyWindow : Window
    {
        ImGuiTextFilter filter;

        void OnUpdate(HE::Timestep ts) override;
        void DrawHierarchy(Assets::Entity parent, Assets::Scene* scene);
        void ContextWindow(Assets::Entity parent);
        void AddNewMenu(Assets::Entity parent);
    };

    struct InspectorWindow : Window
    {
        void OnUpdate(HE::Timestep ts) override;

        template<typename T> void DisplayAddComponentEntry(const std::string& entryName);
    };

    struct AssetManagerWindow : Window
    {
        ImGuiTextFilter filter;

        void OnUpdate(HE::Timestep ts) override;
        void DrawAssetItem(Assets::AssetManager& am, Assets::AssetHandle handle, Assets::AssetMetadata& metaData);
    };

    struct RendererSettingsWindow :  Window
    {
        void OnUpdate(HE::Timestep ts) override;
    };
}