module;

#include "HydraEngine/Base.h"
#include <format>

export module Editor;

import HE;
import Math;
import ImGui;
import Assets;

export namespace Editor {

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

    class EditorCamera
    {
    public:
        CameraTransform transform;
        View view;

        virtual void OnUpdate(HE::Timestep ts) {}
        virtual void SetViewportSize(uint32_t width, uint32_t height) {}
        virtual void Focus(Math::float3 position, float distance) {}
        virtual void UpdateView() {}
    };

    class OrbitCamera : public EditorCamera
    {
    public:
        float rotationSpeed = 4.0f;
        float distance = 5.0f;
        Math::float3 focalPoint = { 0.0f, 0.0f, 0.0f };
        Math::float2 initialMousePosition = { 0.0f, 0.0f };

        OrbitCamera() = default;
        OrbitCamera(float fov, float aspectRatio, float near, float far)
        {
            view.fov = fov;
            view.near = near;
            view.far = far;
            view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);

            UpdateView();
        }

        float ZoomSpeed(float distance)
        {
            float speed = Math::exp(distance / 5.0f) - 1.0f;
            speed = Math::min(Math::max(speed, 0.3f), 100.0f);
            return speed;
        }

        void OnUpdate(HE::Timestep ts) override
        {
            bool isIsWindowHovered = ImGui::IsWindowHovered();

            auto mp = ImGui::GetMousePos();
            Math::float2 mouse{ mp.x, mp.y };
            Math::float2 delta = (mouse - initialMousePosition) * 0.001f;
            initialMousePosition = mouse;

            if (ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyDown(ImGuiKey_MouseMiddle))
            {
                focalPoint -= transform.GetRight() * delta.x * distance;
                focalPoint += transform.GetUp() * delta.y * distance;
            }

            else if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiKey_MouseMiddle))
            {
                distance -= delta.y * ZoomSpeed(distance);
                distance = distance <= 0.0f ? 0.0f : distance;
            }
            else if (ImGui::IsKeyDown(ImGuiKey_MouseMiddle))
            {
                float yaw = -delta.x * rotationSpeed;
                float pitch = -delta.y * rotationSpeed;

                Math::quat yawQuat = Math::angleAxis(yaw, Math::float3(0.0f, 1.0f, 0.0f));

                transform.rotation = yawQuat * transform.rotation;
                transform.rotation = Math::angleAxis(pitch, transform.GetRight()) * transform.rotation;

                transform.rotation = Math::normalize(transform.rotation);
            }

            if (isIsWindowHovered && Math::abs(ImGui::GetIO().MouseWheel) > 0.0f)
            {
                distance -= ImGui::GetIO().MouseWheel * 0.5f;
                UpdateView();
            }

            transform.position = focalPoint + transform.GetForward() * distance;
            UpdateView();
        }

        virtual void SetViewportSize(uint32_t width, uint32_t height) override
        {
            // Update Projection
            if (height == 0) return;
            float aspectRatio = float(width) / float(height);
            view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);
        }

        virtual void Focus(Math::float3 position, float distance) override
        {
            focalPoint = position;
            distance = distance;

            transform.position = focalPoint + transform.GetForward() * distance;

            UpdateView();
        }

        virtual void UpdateView() override
        {
            view.view = Math::inverse(transform.ToMat());
            view.viewProjection = view.projection * view.view;
        }
    };

    class FlyCamera : public EditorCamera
    {
    public:

        float speed = 10.0f;
        Math::float2 initialMousePosition = { 0.0f, 0.0f };

        FlyCamera() = default;
        FlyCamera(float fov, float aspectRatio, float near, float far, float speed = 10.0f)
        {
            view.fov = fov;
            view.near = near;
            view.far = far;
            transform.position = transform.position + transform.GetForward() * 10.0f;
            view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);
            speed = speed;
            UpdateView();
        }

        void OnUpdate(HE::Timestep ts) override
        {
            using ImGuiMouseButton_::ImGuiMouseButton_Right;

            bool isIsWindowHovered = ImGui::IsWindowHovered();

            auto mp = ImGui::GetMousePos();
            Math::float2 mouse{ mp.x, mp.y };
            Math::float2 delta = (mouse - initialMousePosition);
            initialMousePosition = mouse;

            float mouseSensitivity = 0.1f;
            float mult = 1;

            if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
                mult = 4;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                HE::Input::SetCursorMode(HE::Cursor::Mode::Disabled);

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
                HE::Input::SetCursorMode(HE::Cursor::Mode::Normal);

            if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                if (ImGui::IsKeyDown(ImGuiKey_W))
                    transform.position -= transform.GetForward() * Math::float3(ts * speed * mult);
                if (ImGui::IsKeyDown(ImGuiKey_S))
                    transform.position += transform.GetForward() * Math::float3(ts * speed * mult);

                if (ImGui::IsKeyDown(ImGuiKey_D))
                    transform.position += transform.GetRight() * Math::float3(ts * speed * mult);
                if (ImGui::IsKeyDown(ImGuiKey_A))
                    transform.position -= transform.GetRight() * Math::float3(ts * speed * mult);

                if (ImGui::IsKeyDown(ImGuiKey_E))
                    transform.position += transform.GetUp() * Math::float3(ts * speed * mult);
                if (ImGui::IsKeyDown(ImGuiKey_Q))
                    transform.position -= transform.GetUp() * Math::float3(ts * speed * mult);

                {
                    float yaw = -delta.x * ts * mouseSensitivity;
                    float pitch = -delta.y * ts * mouseSensitivity;

                    Math::quat yawQuat = Math::angleAxis(yaw, Math::float3(0.0f, 1.0f, 0.0f));

                    transform.rotation = yawQuat * transform.rotation;
                    transform.rotation = Math::angleAxis(pitch, transform.GetRight()) * transform.rotation;

                    transform.rotation = Math::normalize(transform.rotation);
                }
            }

            if (Math::abs(ImGui::GetIO().MouseWheel) > 0.0f)
            {
                speed = Math::max(1.0f, speed + ImGui::GetIO().MouseWheel * 2.0f);
            }

            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
                transform.position -= delta.y * ts * transform.GetForward();

            if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
            {
                transform.position -= delta.x * ts * transform.GetRight();
                transform.position += delta.y * ts * transform.GetUp();
            }

            UpdateView();
        }

        virtual void Focus(Math::float3 position, float distance) override
        {
            transform.position = position + transform.GetForward() * distance;
            UpdateView();
        }

        virtual void SetViewportSize(uint32_t width, uint32_t height) override
        {
            // Update Projection
            if (height == 0) return;
            float aspectRatio = float(width) / float(height);
            view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);
        }

        virtual void UpdateView() override
        {
            view.view = Math::inverse(transform.ToMat());
            view.viewProjection = view.projection * view.view;
        }
    };


    bool BeginMainMenuBar(bool customTitlebar, ImTextureRef icon, ImTextureRef close, ImTextureRef min, ImTextureRef max, ImTextureRef res)
    {
        if (customTitlebar)
        {
            auto& io = ImGui::GetIO();
            float dpiScale = ImGui::GetWindowDpiScale();
            float scale = ImGui::GetIO().FontGlobalScale * dpiScale;
            ImGuiStyle& style = ImGui::GetStyle();
            static float yframePadding = 8.0f * dpiScale;
            bool isMaximized = HE::Application::GetWindow().IsMaximize();
            auto& title = HE::Application::GetWindow().desc.title;
            bool isIconClicked = false;

            {
                ImGui::ScopedStyle fbs(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGui::ScopedStyle wbs(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::ScopedStyle wr(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 6.0f * dpiScale));
                ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2{ 0, yframePadding });
                ImGui::ScopedColor scWindowBg(ImGuiCol_WindowBg, ImVec4{ 1.0f, 0.0f, 0.0f, 0.0f });
                ImGui::ScopedColor scMenuBarBg(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 1.0f, 0.0f, 0.0f });
                ImGui::BeginMainMenuBar();
            }

            const ImVec2 windowPadding = style.WindowPadding;
            const ImVec2 titlebarMin = ImGui::GetCursorPos() - ImVec2(windowPadding.x, 0);
            const ImVec2 titlebarMax = ImGui::GetCursorPos() + ImGui::GetWindowSize() - ImVec2(windowPadding.x, 0);
            auto* fgDrawList = ImGui::GetForegroundDrawList();
            auto* bgDrawList = ImGui::GetBackgroundDrawList();

            ImGui::SetCursorPos(titlebarMin);

            ImVec2 screenPos = ImGui::GetCursorScreenPos();

            //fgDrawList->AddRect(screenPos + titlebarMin, screenPos + titlebarMax, ImColor32(255, 0, 0, 255));
            //fgDrawList->AddRect(
            //    ImVec2{ screenPos.x + titlebarMax.x / 2 - ImGui::CalcTextSize(title.c_str()).x, screenPos.y + titlebarMin.y },
            //    ImVec2{ screenPos.x + titlebarMax.x / 2 + ImGui::CalcTextSize(title.c_str()).x, screenPos.y + titlebarMax.y },
            //    ImColor32(255, 0, 0, 255)
            //);

            bgDrawList->AddRectFilledMultiColor(
                screenPos + titlebarMin,
                screenPos + titlebarMax,
                ImColorU32(50, 50, 70, 255),
                ImColorU32(50, 50, 50, 255),
                ImColorU32(50, 50, 50, 255),
                ImColorU32(50, 50, 50, 255)
            );

            bgDrawList->AddRectFilledMultiColor(
                screenPos + titlebarMin,
                screenPos + ImGui::GetCursorPos() + ImGui::GetWindowSize() - ImVec2(ImGui::GetWindowSize().x * 3 / 4, 0),
                ImGui::GetColorU32(ImVec4(0.278431f, 0.447059f, 0.701961f, 1.00f)),
                ImColorU32(50, 50, 50, 0),
                ImColorU32(50, 50, 50, 0),
                ImGui::GetColorU32(ImVec4(0.278431f, 0.447059f, 0.701961f, 1.00f))
            );

            if (!ImGui::IsAnyItemHovered() && !HE::Application::GetWindow().IsFullScreen() && ImGui::IsWindowHovered())
            {
                HE::Application::GetWindow().isTitleBarHit = true;
                io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
            }
            else
            {
                HE::Application::GetWindow().isTitleBarHit = false;
                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
            }

            float ySpace = ImGui::GetContentRegionAvail().y;

            // Text
            {
                ImGui::ScopedFont sf(1, 20);
                ImGui::ScopedStyle fr(ImGuiStyleVar_FramePadding, ImVec2{ 8.0f , 8.0f });

                ImVec2 cursorPos = ImGui::GetCursorPos();

                float size = ImGui::CalcTextSize(title.c_str()).x;
                float avail = ImGui::GetContentRegionAvail().x;

                float off = (avail - size) * 0.5f;
                if (off > 0.0f)
                    ImGui::ShiftCursorX(off);

                ImGui::TextUnformatted(title.c_str());

                ImGui::SetCursorPos(cursorPos);
            }

            // icon
            {
                ImVec2 cursorPos = ImGui::GetCursorPos();

                ImGui::ScopedColor sc0(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
                ImGui::ScopedColor sc1(ImGuiCol_ButtonActive, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
                ImGui::ScopedColor sc2(ImGuiCol_ButtonHovered, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

                if (ImGui::ImageButton("icon", icon, { ySpace, ySpace }))
                {
                    isIconClicked = true;
                }

                ImGui::SetCursorPos(cursorPos);
                ImGui::SameLine();

            }

            // buttons
            {
                ImGui::ScopedStyle fr(ImGuiStyleVar_FrameRounding, 0.0f);
                ImGui::ScopedStyle is(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f));
                ImGui::ScopedStyle iis(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0.0f));

                ImVec2 cursorPos = ImGui::GetCursorPos();
                float size = ySpace * 3 + style.FramePadding.x * 6.0f;
                float avail = ImGui::GetContentRegionAvail().x;

                float off = (avail - size);
                if (off > 0.0f)
                    ImGui::ShiftCursorX(off);

                {
                    ImGui::ScopedColor sc0(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
                    if (ImGui::ImageButton("min", min, { ySpace, ySpace })) HE::Application::GetWindow().Minimize();

                    ImGui::SameLine();
                    if (ImGui::ImageButton("max_res", isMaximized ? res : max, { ySpace, ySpace }))
                        isMaximized ? HE::Application::GetWindow().Restore() : HE::Application::GetWindow().Maximize();
                }

                {
                    ImGui::SameLine();
                    ImGui::ScopedColor sc0(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
                    ImGui::ScopedColor sc1(ImGuiCol_ButtonActive, ImVec4{ 1.0f, 0.3f, 0.2f, 1.0f });
                    ImGui::ScopedColor sc2(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.3f, 0.2f, 1.0f });
                    if (ImGui::ImageButton("close", close, { ySpace, ySpace })) HE::Application::Shutdown();
                }

                ImGui::SetCursorPos(cursorPos);
            }

            return isIconClicked;
        }
        else
        {
            ImGui::BeginMainMenuBar();
        }

        return false;
    }

    void EndMainMenuBar()
    {
        ImGui::EndMainMenuBar();
    }

    void BeginChildView(const char* str_id, int& location)
    {
        auto& io = ImGui::GetIO();
        auto& style = ImGui::GetStyle();

        if (location >= 0)
        {
            bool isHiddenTabBar = ImGui::GetCurrentContext()->CurrentWindow->DockNode ? ImGui::GetCurrentContext()->CurrentWindow->DockNode->IsHiddenTabBar() : false;
            float padding = 5 * io.FontGlobalScale;
            ImVec2 PAD = isHiddenTabBar ? padding : ImVec2(padding, ImGui::GetFrameHeight() + padding - 2);
            ImVec2 work_pos = ImGui::GetCurrentContext()->CurrentWindow->Pos;
            ImVec2 work_size = ImGui::GetCurrentContext()->CurrentWindow->Size;
            ImVec2 window_pos, window_pos_pivot;

            window_pos.x = (location & 1) ? (work_pos.x + work_size.x - PAD.x) : (work_pos.x + PAD.x);
            window_pos.y = (location & 2) ? (work_pos.y + work_size.y - PAD.y) : (work_pos.y + PAD.y);
            window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
            window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;

            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        }

        static ImGuiChildFlags flags = ImGuiChildFlags_ResizeY | ImGuiChildFlags_ResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysUseWindowPadding;
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::BeginChild(str_id, ImVec2(0, 0), flags, ImGuiWindowFlags_NoScrollbar);

        {
            ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2{ 8, 8 });
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::MenuItem("Top-left", NULL, location == 0)) { location = 0; }
                if (ImGui::MenuItem("Top-right", NULL, location == 1)) { location = 1; }
                if (ImGui::MenuItem("Bottom-left", NULL, location == 2)) { location = 2; }
                if (ImGui::MenuItem("Bottom-right", NULL, location == 3)) { location = 3; }

                ImGui::EndPopup();
            }
        }
    }

    void EndChildView(int& location)
    {
        ImGui::EndChild();
    }

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

    struct EntityFactory
    {
        std::unordered_map<std::string, std::function<Assets::Entity(Assets::Scene*, Assets::UUID)>> createEnityMapFucntions;
        Assets::AssetManager* am;

        enum Primitive
        {
            Primitive_Capsule,
            Primitive_Cone,
            Primitive_Cube,
            Primitive_Cylinder,
            Primitive_IcoSphere,
            Primitive_Plane,
            Primitive_Suzanne,
            Primitive_Torus,
            Primitive_UVSphere
        };

        void Init(Assets::AssetManager* assetManager)
        {
            am = assetManager;

            auto Empty = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "Empty", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                return newEntity;
            };

            auto PlaneMesh = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "Plane", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
                Assets::AssetHandle handle = am->GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

                auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, Primitive_Plane);

                return newEntity;
            };

            auto CubeMesh = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "Cube", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
                Assets::AssetHandle handle = am->GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

                auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, Primitive_Cube);

                return newEntity;
            };

            auto CapsuleMesh = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "Capsule", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
                Assets::AssetHandle handle = am->GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

                auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, Primitive_Capsule);
                
                return newEntity;
            };

            auto UVSphereMesh = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "UVSphere", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
                Assets::AssetHandle handle = am->GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

                auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, Primitive_UVSphere);

                return newEntity;
            };

            auto IcoSphereMesh = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "IcoSphere", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
                Assets::AssetHandle handle = am->GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

                auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, Primitive_IcoSphere);

                return newEntity;
            };

            auto CylinderMesh = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "Cylinder", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
                Assets::AssetHandle handle = am->GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

                auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, Primitive_Cylinder);

                return newEntity;
            };

            auto ConeMesh = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "Cone", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
                Assets::AssetHandle handle = am->GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

                auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, Primitive_Cone);

                return newEntity;
            };

            auto TorusMesh = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "Torus", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
                Assets::AssetHandle handle = am->GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

                auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, Primitive_Torus);
               
                return newEntity;
            };

            auto Suzanne = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "Suzanne", scene->FindEntity(parent));
                Assets::Entity newEntity = scene->CreateEntity(name, parent);

                std::filesystem::path meshDirectory = std::filesystem::current_path() / "Resources" / "Meshes";
                Assets::AssetHandle handle = am->GetOrMakeAsset(meshDirectory / "Primitives.glb", "Editor/Meshes/Primitives.glb");

                auto& dm = newEntity.AddComponent<Assets::MeshComponent>(handle, Primitive_Suzanne);

                return newEntity;
            };

            //auto EnvironmentMap = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {
            //       
            //    std::string name = GetIncrementedReletiveEntityName(scene, "Environment Map", scene->FindEntity(parent));
            //    Assets::Entity newEntity = scene->CreateEntity(name, parent);
            //
            //    auto& em = newEntity.AddComponent<Assets::EnvironmentMapComponent>();
            //    auto& tc = newEntity.GetComponent<Assets::TransformComponent>();
            //    tc.rotation = glm::vec3(0, 0.0f, 0.0f);
            //
            //    return newEntity;
            //};

            auto DirectionalLight = [this](Assets::Scene* scene, Assets::UUID parent) -> Assets::Entity {

                std::string name = GetIncrementedReletiveEntityName(scene, "Directional Light", scene->FindEntity(parent));
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

            createEnityMapFucntions = {

                { "Empty"                , Empty },
                { "Mesh/Plane"           , PlaneMesh },
                { "Mesh/Cube"            , CubeMesh },
                { "Mesh/Capsule"         , CapsuleMesh },
                { "Mesh/UVSphere"        , UVSphereMesh },
                { "Mesh/IcoSphere"       , IcoSphereMesh },
                { "Mesh/Cylinder"        , CylinderMesh },
                { "Mesh/Cone"            , ConeMesh },
                { "Mesh/Torus"           , TorusMesh },
                { "Mesh/Suzanne"         , Suzanne },
                //{ "Light/EnvironmentMap" , EnvironmentMap },
                { "Light/Directional"    , DirectionalLight },
                //{ "Light/Point"			 , PointLight },
                //{ "Light/Spot"			 , SpotLight },
            };
        }

        Assets::Entity CreateEntity(Assets::Scene* scene, const std::string& key, Assets::UUID parent)
        {
            if (createEnityMapFucntions.contains(key))
                return createEnityMapFucntions.at(key)(scene, parent);

            return {};
        }

        void AddFactory(const std::string& key, std::function<Assets::Entity(Assets::Scene* scene, Assets::UUID)> function)
        {
            createEnityMapFucntions[key] = function;
        }

        std::string IncrementString(const std::string& str)
        {
            std::string input = str;
            uint32_t number = 0;
            uint32_t multiplier = 1;

            for (size_t i = input.length() - 1; i > 0; --i)
            {
                auto c = input[i];
                if (std::isdigit(c))
                {
                    number += (input[i] - '0') * multiplier;
                    multiplier *= 10;
                }
                else if (number != 0)
                {
                    // If a non-digit character is encountered after we've started
                    // accumulating the number, break the loop.
                    break;
                }
            }

            std::string strNumber = std::to_string(number + 1);

            std::string result = input;
            size_t pos = result.find_last_of("0123456789"); // Finding the position of the last digit
            if (pos != std::string::npos)
            {
                while (pos > 0 && std::isdigit(result[pos - 1]))
                    pos--; // Move back if it's a multi-digit number

                result.replace(pos, result.length() - pos, strNumber); // Replace the number
            }
            else
            {
                result += " " + strNumber;
            }

            return result;
        }

        bool IsEntityNameExistInChildren(Assets::Scene* scene, const std::string& name, Assets::Entity entity)
        {
            for (auto id : entity.GetChildren())
            {
                Assets::Entity entity = scene->FindEntity(id);
                if (entity.GetName() == name)
                    return true;
            }

            return false;
        }

        std::string GetIncrementedReletiveEntityName(Assets::Scene* scene, const std::string& name, Assets::Entity parent)
        {
            if (!IsEntityNameExistInChildren(scene, name, parent))
                return  name;

            std::string result = IncrementString(name);
            return GetIncrementedReletiveEntityName(scene, result, parent); // keep Incrementing the number
        }
    };
}