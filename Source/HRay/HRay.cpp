#include "HydraEngine/Base.h"
#include <format>
#include "Embeded/icon.h"

#if NVRHI_HAS_D3D12
#include "Embeded/dxil/Main.bin.h"
#endif

#if NVRHI_HAS_VULKAN
#include "Embeded/spirv/Main.bin.h"
#endif

import HE;
import std;
import Math;
import nvrhi;
import ImGui;
import Utils;
import Editor;

using namespace HE;

namespace HRay {

    struct ViewBuffer
    {
        Math::float4x4 clipToWorld;

        Math::float3 cameraPosition;
        int padding;

        Math::float2 viewSize;
        Math::float2 viewSizeInv;
    };

    struct RendererData
    {
        nvrhi::DeviceHandle device;
        nvrhi::ShaderLibraryHandle shaderLibrary;
        nvrhi::BindingLayoutHandle bindingLayout;
        nvrhi::BindingSetHandle bindingSet;
        nvrhi::TextureHandle renderTarget;
        nvrhi::BufferHandle viewBuffer;
        nvrhi::rt::PipelineHandle pipeline;
        nvrhi::rt::ShaderTableHandle shaderTable;
        nvrhi::rt::AccelStructHandle topLevelAS;
        nvrhi::rt::AccelStructHandle bottomLevelAS;
    };

    void Init(RendererData& data, nvrhi::DeviceHandle pDevice, nvrhi::CommandListHandle commandList)
    {
        data.device = pDevice;

        data.shaderLibrary = RHI::CreateShaderLibrary(data.device, STATIC_SHADER(Main), nullptr);
        HE_VERIFY(data.shaderLibrary);

        data.viewBuffer = data.device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ViewBuffer), "ViewBuffer", sizeof(ViewBuffer)));
        HE_VERIFY(data.viewBuffer);

        nvrhi::BindingLayoutDesc globalBindingLayoutDesc;
        globalBindingLayoutDesc.visibility = nvrhi::ShaderType::All;
        globalBindingLayoutDesc.bindings = {
            { 0, nvrhi::ResourceType::RayTracingAccelStruct },
            { 0, nvrhi::ResourceType::Texture_UAV },
            { 0, nvrhi::ResourceType::VolatileConstantBuffer }
        };

        data.bindingLayout = data.device->createBindingLayout(globalBindingLayoutDesc);

        nvrhi::rt::PipelineDesc pipelineDesc;
        pipelineDesc.globalBindingLayouts = { data.bindingLayout };
        pipelineDesc.shaders = {
            { "", data.shaderLibrary->getShader("RayGen", nvrhi::ShaderType::RayGeneration), nullptr },
            { "", data.shaderLibrary->getShader("Miss", nvrhi::ShaderType::Miss), nullptr }
        };

        pipelineDesc.hitGroups = { {
            "HitGroup",
            data.shaderLibrary->getShader("ClosestHit", nvrhi::ShaderType::ClosestHit),
            nullptr,
            nullptr,
            nullptr,
            false
        } };

        pipelineDesc.maxPayloadSize = sizeof(Math::float4);
        data.pipeline = data.device->createRayTracingPipeline(pipelineDesc);
        
        data.shaderTable = data.pipeline->createShaderTable();
        data.shaderTable->setRayGenerationShader("RayGen");
        data.shaderTable->addHitGroup("HitGroup");
        data.shaderTable->addMissShader("Miss");
        
        nvrhi::BufferDesc bufferDesc;
        bufferDesc.byteSize = sizeof(uint32_t) * 3;
        bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        bufferDesc.keepInitialState = true;
        bufferDesc.isAccelStructBuildInput = true;
        nvrhi::BufferHandle indexBuffer = data.device->createBuffer(bufferDesc);
        bufferDesc.byteSize = sizeof(Math::float3) * 3;
        nvrhi::BufferHandle vertexBuffer = data.device->createBuffer(bufferDesc);

        uint32_t indices[3] = { 0, 1, 2 };
        Math::float3 vertices[3] = { Math::float3(-1, -1, 0), Math::float3(1, -1, 0), Math::float3(0, 1, 0) };
       
        commandList->writeBuffer(indexBuffer, indices, sizeof(indices));
        commandList->writeBuffer(vertexBuffer, vertices, sizeof(vertices));

        nvrhi::rt::AccelStructDesc blasDesc;
        blasDesc.isTopLevel = false;
        nvrhi::rt::GeometryDesc geometryDesc;
        auto& triangles = geometryDesc.geometryData.triangles;
        triangles.indexBuffer = indexBuffer;
        triangles.vertexBuffer = vertexBuffer;
        triangles.indexFormat = nvrhi::Format::R32_UINT;
        triangles.indexCount = 3;
        triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
        triangles.vertexStride = sizeof(Math::float3);
        triangles.vertexCount = 3;
        geometryDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
        geometryDesc.flags = nvrhi::rt::GeometryFlags::Opaque;
        blasDesc.bottomLevelGeometries.push_back(geometryDesc);

        data.bottomLevelAS = data.device->createAccelStruct(blasDesc);
        nvrhi::utils::BuildBottomLevelAccelStruct(commandList, data.bottomLevelAS, blasDesc);

        nvrhi::rt::AccelStructDesc tlasDesc;
        tlasDesc.isTopLevel = true;
        tlasDesc.topLevelMaxInstances = 1;

        data.topLevelAS = data.device->createAccelStruct(tlasDesc);

        nvrhi::rt::InstanceDesc instanceDesc;
        Math::float4x4 transform = Math::float4x4(1.0f);
        memcpy(instanceDesc.transform, &transform, sizeof(transform));

        instanceDesc.bottomLevelAS = data.bottomLevelAS;
        instanceDesc.instanceMask = 1;
        instanceDesc.flags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;

        commandList->buildTopLevelAccelStruct(data.topLevelAS, &instanceDesc, 1);
    }

    void CreateResources(RendererData& data, uint32_t width, uint32_t height)
    {
        data.bindingSet.Reset();
        data.renderTarget.Reset();

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = width;
        textureDesc.height = height;
        textureDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
        textureDesc.format = nvrhi::Format::RGBA8_UNORM;
        textureDesc.keepInitialState = true;
        textureDesc.isRenderTarget = false;
        textureDesc.isUAV = true;
        data.renderTarget = data.device->createTexture(textureDesc);

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.bindings = {
            nvrhi::BindingSetItem::RayTracingAccelStruct(0, data.topLevelAS),
            nvrhi::BindingSetItem::Texture_UAV(0, data.renderTarget),
            nvrhi::BindingSetItem::ConstantBuffer(0, data.viewBuffer)
        };

        data.bindingSet = data.device->createBindingSet(bindingSetDesc, data.bindingLayout);
    }

    void SumbitToGPU(RendererData& data, nvrhi::ICommandList* commandList, Editor::EditorCamera* editorCamera, uint32_t width, uint32_t height)
    {
        // ViewBuffer
        {
            ViewBuffer buffer;
            buffer.clipToWorld = editorCamera->transform.ToMat() * Math::inverse(editorCamera->view.projection);
            buffer.cameraPosition = editorCamera->transform.position;
            buffer.viewSize = Math::float2(width, height);
            buffer.viewSizeInv = 1.0f / Math::float2(width, height);

            commandList->writeBuffer(data.viewBuffer, &buffer, sizeof(buffer));
        }

        nvrhi::rt::State state;
        state.shaderTable = data.shaderTable;
        state.bindings = { data.bindingSet };
        commandList->setRayTracingState(state);

        nvrhi::rt::DispatchRaysArguments args;
        args.width = width;
        args.height = height;
        commandList->dispatchRays(args);
    }
}

class HRayApp : public Layer
{
public:
    nvrhi::DeviceHandle device;
    nvrhi::CommandListHandle commandList;
    nvrhi::TextureHandle icon, close, min, max, res;

    HRay::RendererData rd;

    Scope<Editor::EditorCamera> editorCamera;
    int width = 1920, height = 1080;
    bool enableTitlebar = true;
    bool enableViewPortWindow = true;
    bool enableSettingWindow = true;
    bool useViewportSize = true;

    virtual void OnAttach() override
    {
        device = RHI::GetDevice();
        commandList = device->createCommandList();

        Plugins::LoadPluginsInDirectory("Plugins");

        editorCamera = CreateScope<Editor::OrbitCamera>(60.0f, float(width) / float(height), 0.1f, 100.0f);

        commandList->open();

        HRay::Init(rd, device, commandList);
        HRay::CreateResources(rd, width, height);

        // icons
        {
            HE_PROFILE_SCOPE("Load Icons");

            icon = Utils::LoadTexture(Application::GetApplicationDesc().windowDesc.iconFilePath, device, commandList);
            close = Utils::LoadTexture(Buffer(g_icon_close, sizeof(g_icon_close)), device, commandList);
            min = Utils::LoadTexture(Buffer(g_icon_minimize, sizeof(g_icon_minimize)), device, commandList);
            max = Utils::LoadTexture(Buffer(g_icon_maximize, sizeof(g_icon_maximize)), device, commandList);
            res = Utils::LoadTexture(Buffer(g_icon_restore, sizeof(g_icon_restore)), device, commandList);
        }

        commandList->close();
        device->executeCommandList(commandList);
    }

    virtual void OnUpdate(const FrameInfo& info) override
    {
        // UI
        {
            HE_PROFILE_SCOPE_NC("UI", 0xFF0000FF);

            float dpi = ImGui::GetWindowDpiScale();
            float scale = ImGui::GetIO().FontGlobalScale * dpi;
            auto& style = ImGui::GetStyle();
            style.FrameRounding = Math::max(3.0f * scale, 1.0f);
            ImGui::ScopedStyle wb(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar);

            if (enableTitlebar)
            {
                bool customTitlebar = Application::GetApplicationDesc().windowDesc.customTitlebar;
                bool isIconClicked = Utils::BeginMainMenuBar(customTitlebar, icon.Get(), close.Get(), min.Get(), max.Get(), res.Get());

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

                if (ImGui::BeginMenu("Window"))
                {
                    if (ImGui::MenuItem("Title Bar", "Left Shift + T", enableTitlebar))
                        enableTitlebar = enableTitlebar ? false : true;

                    if (ImGui::MenuItem("View Port", "Left Shift + 1", enableViewPortWindow))
                        enableViewPortWindow = enableViewPortWindow ? false : true;

                    if (ImGui::MenuItem("Setting", "Left Shift + 2", enableSettingWindow))
                        enableSettingWindow = enableSettingWindow ? false : true;

                    ImGui::EndMenu();
                }

                Utils::EndMainMenuBar();
            }

            if (enableViewPortWindow)
            {
                ImGui::ScopedStyle ss(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f,0.0f });
                if (ImGui::Begin("View"))
                {
                    auto size = ImGui::GetContentRegionAvail();
                    uint32_t w = Utils::Align(uint32_t(size.x), 2u);
                    uint32_t h = Utils::Align(uint32_t(size.y), 2u);

                    if (useViewportSize && (width != w || height != h))
                    {
                        width = w;
                        height = h;
                        Jops::SubmitToMainThread([this]() { HRay::CreateResources(rd, width, height); });
                    }

                    editorCamera->SetViewportSize(width, height);
                    editorCamera->OnUpdate(info.ts);

                    ImGui::Image(rd.renderTarget.Get(), ImVec2{ (float)size.x, (float)size.y });
                }
                ImGui::End();
            }

            if (enableSettingWindow)
            {
                auto cf = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

                if (ImGui::Begin("Setting", &enableSettingWindow))
                {
                    ImGui::ScopedStyle ss(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
                    ImGui::ScopedColor sc0(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
                    ImGui::ScopedColor sc1(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
                    ImGui::ScopedColor sc2(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
                    
                    ImGui::BeginChild("Appliction", { 0,0 }, cf);
                    if (ImGui::CollapsingHeader("Appliction", ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        const auto& appStats = Application::GetStats();
                        ImGui::Text("Graphics API : %s", nvrhi::utils::GraphicsAPIToString(device->getGraphicsAPI()));
                        ImGui::Text("FPS : %zd", appStats.FPS);
                        ImGui::Text("CPUMainTime %f ms", appStats.CPUMainTime);
                    }
                    ImGui::EndChild();

                    ImGui::BeginChild("Setting", { 0,0 }, cf);

                    if (ImGui::CollapsingHeader("Setting", ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::Checkbox("Use ViewPort Size", &useViewportSize);
                        if (!useViewportSize)
                        {
                            if (ImGui::DragInt("Width", &width, 1))
                            {
                                width = Utils::Align(uint32_t(width), 2u);
                                Jops::SubmitToMainThread([this]() { HRay::CreateResources(rd, width, height); });
                            }

                            if (ImGui::DragInt("Height", &height, 1))
                            {
                                height = Utils::Align(uint32_t(height), 2u);
                                Jops::SubmitToMainThread([this]() { HRay::CreateResources(rd, width, height); });
                            }
                        }
                        else
                        {
                            ImGui::Text("Width  : %zd", width);
                            ImGui::Text("Height : %zd", height);
                        }
                    }
                    ImGui::EndChild();
                }

                ImGui::End();
            }
        }

        {
            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::Escape))
                Application::Restart();

            if (Input::IsKeyPressed(Key::Escape))
                Application::Shutdown();

            if (Input::IsKeyReleased(Key::F))
                Application::GetWindow().ToggleScreenState();

            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::T))
                enableTitlebar = enableTitlebar ? false : true;

            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::D1))
                enableViewPortWindow = enableViewPortWindow ? false : true;

            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::D2))
                enableSettingWindow = enableSettingWindow ? false : true;

            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyReleased(Key::M))
            {
                if (!Application::GetWindow().IsMaximize())
                    Application::GetWindow().MaximizeWindow();
                else
                    Application::GetWindow().RestoreWindow();
            }

            if (Input::IsKeyPressed(Key::H))
            {
                if (Input::GetCursorMode() == Cursor::Mode::Disabled)
                    Input::SetCursorMode(Cursor::Mode::Normal);
                else
                    Input::SetCursorMode(Cursor::Mode::Disabled);
            }


            if (Input::IsKeyDown(Key::LeftAlt) && Input::IsKeyReleased(Key::D1))
                editorCamera = CreateScope<Editor::OrbitCamera>(60.0f, float(width) / float(height), 0.01f, 1000.0f);

            if (Input::IsKeyDown(Key::LeftAlt) && Input::IsKeyReleased(Key::D2))
                editorCamera = CreateScope<Editor::FlyCamera>(60.0f, float(width) / float(height), 0.01f, 1000.0f);
        }

        {
            HRay::SumbitToGPU(rd, commandList, editorCamera.get(), width, height);
        }
    }

    virtual void OnBegin(const FrameInfo& info) override
    {
        commandList->open();
        nvrhi::utils::ClearColorAttachment(commandList, info.fb, 0, nvrhi::Color(0.1f));
    }

    virtual void OnEnd(const FrameInfo& info) override
    {
        commandList->close();
        device->executeCommandList(commandList);
    }
};

HE::ApplicationContext* HE::CreateApplication(ApplicationCommandLineArgs args)
{
    ApplicationDesc desc;

    desc.deviceDesc.enableRayTracingExtensions = true;
    desc.deviceDesc.api = {
        nvrhi::GraphicsAPI::D3D12,
        nvrhi::GraphicsAPI::VULKAN,
    };

    desc.windowDesc.title = "HRay";
    desc.windowDesc.iconFilePath = "Resources/Icons/Hydra.png";
    desc.windowDesc.customTitlebar = true;
    desc.windowDesc.minWidth = 960;
    desc.windowDesc.minHeight = 540;

    ApplicationContext* ctx = new ApplicationContext(desc);
    Application::PushLayer(new HRayApp());

    return ctx;
}

#include "HydraEngine/EntryPoint.h"