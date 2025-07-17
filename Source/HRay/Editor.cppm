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

export namespace Editor {

    //////////////////////////////////////////////////////////////////////////
    // Window
    //////////////////////////////////////////////////////////////////////////

    using WindowHandle = uint64_t;

    struct WindowDesc
    {
        std::string title;
        std::string invokePath;
        std::string tooltip;
        bool menuBar = false;
        ImVec2 padding = { 1, 1};
        ImVec2 size;
    };

    struct Window
    {
        virtual ~Window() {}
        virtual void OnCreate() {}
        virtual void OnUpdate(HE::Timestep ts) {}
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

    struct WindowManager
    {
        std::vector<WindowScript> scripts;
        std::vector<WindowDesc> descs;
        std::map<std::string, bool> state;

        WindowManager()
        {
            scripts.reserve(100);
            descs.reserve(100);
        }
    };

    void DistroyWindow(WindowHandle handle);
    void SaveWindowsState();
    void UpdateWindows(HE::Timestep ts);
    void CloseAllWindows();
    WindowDesc& GetWindowDesc(WindowHandle handle);
    WindowDesc& GetWindowDesc(Window* window);
    void SerializeWindows(std::ostringstream& out);
    void DeserializeWindows(simdjson::dom::element element);
    void DeserializeWindow(WindowHandle handle);

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

    struct App;
    struct Context
    {
        App* app;
        nvrhi::DeviceHandle device;
        nvrhi::CommandListHandle commandList;
        nvrhi::TextureHandle icon, close, min, max, res, board;

        Assets::AssetManager assetManager;
        Assets::SubscriberHandle assetEventCallbackHandle = 0;
        Assets::AssetHandle sceneHandle = 0;
        HRay::RendererData rd;

        WindowManager windowManager;

        std::unordered_map<std::string, std::function<Assets::Entity(Assets::Scene*, Assets::UUID)>> createEnityFucntions;

        ImGuiTextFilter textFilter;

        ImVec4 colors[Color::Count];
        float fontScale = 1.0f;

        Assets::Entity selectedEntity;
        int gizmoType = ImGuizmo::TRANSLATE;

        bool enableTitlebar = true;
        bool enableStartMenu = true;
        bool enableCreateNewProjectPopub = false;
        bool importing = false;

        std::string projectPath;
        std::string projectName;
        std::filesystem::path appData;
        std::filesystem::path keyBindingsFilePath;
        Project project;

        Assets::AssetHandle tempSceneHandle;
        std::string outputPath;
        SceneMode sceneMode = SceneMode::Editor;

        int width = 1920;
        int height = 1080;
        bool useViewportSize = true;

        int frameStart = 0;
        int frameEnd = 50;
        int frameStep = 1;
        int maxSamples = 1024;
        
        int sampleCount = 0;
        int frameIndex = 0;
    };
   
    struct App : public HE::Layer, public Assets::AssetEventCallback
    {
        inline static Context* context;

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

    Assets::Entity GetSceneCamera(Assets::Scene* scene);
   
    void SetRendererToSceneCameraProp(const Assets::CameraComponent& c);
    void SetRendererToEditorCameraProp();

    void Animate();
    void Stop();

    void ImportModel(const std::filesystem::path& file);
    void OpenProject(const std::filesystem::path& file);
    void CreateNewProject(const std::filesystem::path& path, std::string projectName);
    void OpenScene();
    void NewScene();

    void Save(nvrhi::IDevice* device, nvrhi::ITexture* texture, const std::string& directory, uint32_t frameIndex);
    void ImportMeshSource(Assets::Scene* scene, Assets::Entity parent, Assets::Node& node, Assets::Asset asset)
    {
        for (auto& node : node.GetChildren(asset.Get<Assets::MeshSourecHierarchy>()))
        {
            Assets::Entity newEntity = scene->CreateEntity(node.name, parent.GetUUID());
    
            if (node.meshIndex != Assets::c_Invalid)
            {
                auto& meshSource = asset.Get<Assets::MeshSource>();
                auto& mesh = meshSource.meshes[node.meshIndex];
    
                auto& dmc = newEntity.AddComponent<Assets::MeshComponent>();
                dmc.meshSourceHandle = asset.GetHandle();
                dmc.meshIndex = node.meshIndex;
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
        auto index = windowManager.scripts.size();

        windowManager.descs.push_back(desc);

        auto& script = windowManager.scripts.emplace_back();
        script.Bind<T>(std::forward<Args>(args)...);

        return index;
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
    void BeginChildView(const char* str_id, Corner& location);
    void EndChildView();

    std::string IncrementString(const std::string& str);


    //////////////////////////////////////////////////////////////////////////
    // App Windows
    //////////////////////////////////////////////////////////////////////////

    struct ViewPortWindow : Window, SceneCallback, SerializationCallback
    {
        HE::Ref<OrbitCamera> orbitCamera;
        HE::Ref<FlyCamera> flyCamera;
        HE::Ref<EditorCamera> editorCamera;
        int width = 1920, height = 1080;
        Math::vec2 viewportBounds[2];
        
        Animation cameraAnimation;
        bool previewMode = false;

        Math::float3 cameraPrevPos;
        Math::quat cameraPrevRot;
        bool clearReq = false;

        Corner toolbarCorner = Corner::TopLeft;    // 0
        Corner axisGizmoCorner = Corner::TopRight; // 1
        Corner statsCorner = Corner::ButtomLeft;   // 2

        void OnCreate() override;
        void OnUpdate(HE::Timestep ts) override;
        void Serialize(std::ostringstream& out) override;
        void Deserialize(simdjson::dom::element element) override;
        void OnAnimationStart() override;
        void OnAnimationStop() override;

        void UpdateEditorCameraAnimation(Assets::Scene* scene, Assets::Entity mainCameraEntity, float ts);
        void Preview();
        void StopPreview(bool moveBack = true);

        void SetFlyCamera();
        void SetOrbitCamera();
        void FocusCamera();
        void AlignActiveCameraToView(Assets::Scene* scene);
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
        bool TextureHandler(Assets::Material* mat, Assets::AssetHandle handle);

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