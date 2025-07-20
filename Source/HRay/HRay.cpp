#include <HydraEngine/Base.h>

#if NVRHI_HAS_D3D12
#include "Embeded/dxil/Main.bin.h"
#include "Embeded/dxil/PostProcessing_Main.bin.h"
#endif

#if NVRHI_HAS_VULKAN
#include "Embeded/spirv/Main.bin.h"
#include "Embeded/spirv/PostProcessing_Main.bin.h"
#endif

import HRay;
import nvrhi;
import HE;
import Assets;
import std;

constexpr int c_DefaultMaterialIndex = 0;

struct MeshSourceBuffers
{
    nvrhi::BufferHandle indexBuffer;
    nvrhi::BufferHandle vertexBuffer;
};

struct MeshSourceDescriptors
{
    Assets::DescriptorHandle indexBufferDescriptor;
    Assets::DescriptorHandle vertexBufferDescriptor;
};

// Reference: https://github.com/google/filament/blob/b15ae15f39181e1a16fe248bd022fe4c36eab6de/filament/src/Exposure.cpp#L150
static float Luminance(float const ev100) noexcept 
{
    // With L the average scene luminance, S the sensitivity and K the
    // reflected-light meter calibration constant:
    //
    // EV = log2(L * S / K)
    // L = 2^EV100 * K / 100
    //
    // As in ev100FromLuminance(luminance), we use K = 12.5 to match common camera
    // manufacturers (Canon, Nikon and Sekonic):
    //
    // L = 2^EV100 * 12.5 / 100 = 2^EV100 * 0.125
    //
    // With log2(0.125) = -3 we have:
    //
    // L = 2^(EV100 - 3)
    //
    // Reference: https://en.wikipedia.org/wiki/Exposure_value
    return std::pow(2.0f, ev100 - 3.0f);
}

static void CreateOrResizeRenderTarget(HRay::RendererData& data, HRay::FrameData& frameData, uint32_t width, uint32_t height)
{
    HE_PROFILE_FUNCTION();


    nvrhi::TextureDesc textureDesc;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    textureDesc.format = nvrhi::Format::RGBA16_UNORM;
    textureDesc.keepInitialState = true;
    textureDesc.isRenderTarget = false;
    textureDesc.isUAV = true;
    frameData.renderTarget = data.device->createTexture(textureDesc);
    frameData.prevRenderTarget = data.device->createTexture(textureDesc);

    //frameData.renderTarget.Reset();
    frameData.bindingSet.Reset();
}

static void CreateOrResizeGeoBuffer(HRay::RendererData& data, HRay::FrameData& frameData, uint32_t newSize)
{
    HE_PROFILE_FUNCTION();

    frameData.geometryData.resize(newSize);
    nvrhi::BufferDesc bufferDesc;
    bufferDesc.byteSize = sizeof(HRay::GeometryData) * frameData.geometryData.size();
    bufferDesc.debugName = "Geometry";
    bufferDesc.structStride = sizeof(HRay::GeometryData);
    bufferDesc.canHaveRawViews = true;
    bufferDesc.canHaveUAVs = true;
    bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    bufferDesc.keepInitialState = true;
    frameData.geometryBuffer = data.device->createBuffer(bufferDesc);

    frameData.bindingSet.Reset();
}

static void CreateOrResizeInstanceBuffer(HRay::RendererData& data, HRay::FrameData& frameData, uint32_t newSize)
{
    HE_PROFILE_FUNCTION();

    // topLevelAS
    {
        frameData.instances.resize(newSize);
        const size_t maxInstancesCount = frameData.instances.size();
        nvrhi::rt::AccelStructDesc tlasDesc;
        tlasDesc.debugName = "TLAS";
        tlasDesc.isTopLevel = true;
        tlasDesc.topLevelMaxInstances = maxInstancesCount;
        frameData.topLevelAS = data.device->createAccelStruct(tlasDesc);
        HE_ASSERT(frameData.topLevelAS);
    }

    // instanceBuffer
    {
        frameData.instanceData.resize(newSize);
        nvrhi::BufferDesc bufferDesc;
        bufferDesc.byteSize = sizeof(HRay::InstanceData) * frameData.instanceData.size();
        bufferDesc.debugName = "Instances";
        bufferDesc.structStride = sizeof(HRay::InstanceData);
        bufferDesc.canHaveRawViews = true;
        bufferDesc.canHaveUAVs = true;
        bufferDesc.isVertexBuffer = true;
        bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        bufferDesc.keepInitialState = true;
        frameData.instanceBuffer = data.device->createBuffer(bufferDesc);
        HE_ASSERT(frameData.instanceBuffer);
    }

    frameData.bindingSet.Reset();
}

static void CreateOrResizeMaterialBuffer(HRay::RendererData& data, HRay::FrameData& frameData, uint32_t newSize)
{
    HE_PROFILE_FUNCTION();

    frameData.materialData.resize(newSize);
    nvrhi::BufferDesc bufferDesc;
    bufferDesc.byteSize = frameData.materialData.size() * sizeof(HRay::MaterialData);
    bufferDesc.debugName = "MaterialBuffer";
    bufferDesc.structStride = sizeof(HRay::MaterialData);
    bufferDesc.canHaveRawViews = true;
    bufferDesc.canHaveUAVs = true;
    bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    bufferDesc.keepInitialState = true;
    frameData.materialBuffer = data.device->createBuffer(bufferDesc);

    frameData.bindingSet.Reset();
}

static void CreateOrResizeDirectionalLightBuffer(HRay::RendererData& data, HRay::FrameData& frameData, uint32_t newSize)
{
    HE_PROFILE_FUNCTION();

    frameData.directionalLightData.resize(newSize);
    nvrhi::BufferDesc bufferDesc;
    bufferDesc.byteSize = frameData.directionalLightData.size() * sizeof(HRay::DirectionalLightData);
    bufferDesc.debugName = "Directional Light Buffer";
    bufferDesc.structStride = sizeof(HRay::DirectionalLightData);
    bufferDesc.canHaveRawViews = true;
    bufferDesc.canHaveUAVs = true;
    bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    bufferDesc.keepInitialState = true;
    frameData.directionalLightBuffer = data.device->createBuffer(bufferDesc);

    frameData.bindingSet.Reset();
}

void HRay::Init(RendererData& data, nvrhi::DeviceHandle pDevice, nvrhi::CommandListHandle commandList)
{
    HE_PROFILE_FUNCTION();

    data.device = pDevice;

    // Descriptor Table Manager
    {
        HE_PROFILE_SCOPE("Create Descriptor Table Manager");

        nvrhi::BindlessLayoutDesc bindlessLayoutDesc;
        bindlessLayoutDesc.visibility = nvrhi::ShaderType::All;
        bindlessLayoutDesc.firstSlot = 0;
        bindlessLayoutDesc.maxCapacity = 1024;
        bindlessLayoutDesc.registerSpaces = {
            nvrhi::BindingLayoutItem::RawBuffer_SRV(1),
            nvrhi::BindingLayoutItem::Texture_SRV(2)
        };
        data.bindlessLayout = data.device->createBindlessLayout(bindlessLayoutDesc);
        data.descriptorTable = HE::CreateRef<Assets::DescriptorTableManager>(data.device, data.bindlessLayout);
    }

    // Shaders
    {
        HE_PROFILE_SCOPE("CreateShaderLibrary");

        data.shaderLibrary = HE::RHI::CreateShaderLibrary(data.device, STATIC_SHADER(Main), nullptr);
        HE_VERIFY(data.shaderLibrary);

        nvrhi::ShaderDesc desc;
        desc.shaderType = nvrhi::ShaderType::Compute;
        desc.entryName = "Main";
        data.cs = HE::RHI::CreateStaticShader(data.device, STATIC_SHADER(PostProcessing_Main), nullptr, desc);
    }

    // Samplers
    {
        HE_PROFILE_SCOPE("create Samplers");

        auto samplerDesc = nvrhi::SamplerDesc()
            .setAllFilters(false)
            .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp)
            .setAllFilters(true)
            .setAllAddressModes(nvrhi::SamplerAddressMode::Wrap)
            .setMaxAnisotropy(16);
        data.anisotropicWrapSampler = data.device->createSampler(samplerDesc);
        HE_VERIFY(data.anisotropicWrapSampler);
    }

    // Global Binding Layout
    {
        HE_PROFILE_SCOPE("createBindingLayout");

        nvrhi::BindingLayoutDesc desc;
        desc.visibility = nvrhi::ShaderType::All;
        desc.bindings = {
           nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
           nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),
           nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2),
           nvrhi::BindingLayoutItem::StructuredBuffer_SRV(3),
           nvrhi::BindingLayoutItem::StructuredBuffer_SRV(4),
           nvrhi::BindingLayoutItem::Texture_UAV(0),
           nvrhi::BindingLayoutItem::Texture_UAV(1),
           nvrhi::BindingLayoutItem::Sampler(0),
           nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)
        };

        data.bindingLayout = data.device->createBindingLayout(desc);
        HE_ASSERT(data.bindingLayout);
    }

    // PostProcessing Binding Layout
    {
        HE_PROFILE_SCOPE("createBindingLayout");

        nvrhi::BindingLayoutDesc desc;
        desc.visibility = nvrhi::ShaderType::All;
        desc.bindings = {
           nvrhi::BindingLayoutItem::Texture_UAV(0),
           //nvrhi::BindingLayoutItem::PushConstants(0, sizeof(uint32_t))
        };

        data.postProcessingBindingLayout = data.device->createBindingLayout(desc);
        HE_ASSERT(data.postProcessingBindingLayout);
    }

    // Pipeline
    {
        HE_PROFILE_SCOPE("createRayTracingPipeline");

        nvrhi::rt::PipelineDesc pipelineDesc;
        pipelineDesc.globalBindingLayouts = { data.bindingLayout, data.bindlessLayout };
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

        pipelineDesc.maxPayloadSize = sizeof(Math::float4) + sizeof(Math::float3) * 2 + sizeof(float) * 3;
        data.pipeline = data.device->createRayTracingPipeline(pipelineDesc);
        HE_VERIFY(data.pipeline);

        data.shaderTable = data.pipeline->createShaderTable();
        data.shaderTable->setRayGenerationShader("RayGen");
        data.shaderTable->addMissShader("Miss");
        data.shaderTable->addHitGroup("HitGroup");
    }

    // Compute
    {
        data.computePipeline = data.device->createComputePipeline({ data.cs, { data.postProcessingBindingLayout } }); 
        HE_VERIFY(data.computePipeline);
    }
}

void HRay::BeginScene(RendererData& data, FrameData& frameData)
{
    HE_PROFILE_FUNCTION();

    if (!frameData.renderTarget)
        CreateOrResizeRenderTarget(data, frameData, 1920, 1080);

    if (!frameData.geometryBuffer)
        CreateOrResizeGeoBuffer(data, frameData, 1024);

    if (!frameData.instanceBuffer)
        CreateOrResizeInstanceBuffer(data, frameData, 1024);

    if (!frameData.materialBuffer)
        CreateOrResizeMaterialBuffer(data, frameData, 1024);

    if (!frameData.directionalLightBuffer)
        CreateOrResizeDirectionalLightBuffer(data, frameData, 2);

    if (!frameData.sceneInfoBuffer)
    {
        HE_PROFILE_SCOPE("Create SceneInfo Buffer");

        frameData.sceneInfoBuffer = data.device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(SceneInfo), "SceneInfoBuffer", sizeof(SceneInfo)));
        HE_VERIFY(frameData.sceneInfoBuffer);
    }

    frameData.geometryCount = 0;
    frameData.instanceCount = 0;
    frameData.materialCount = 1; // 0 for DefaultMaterial
    frameData.sceneInfo.light.directionalLightCount = 0;

    {
        frameData.materials.clear();
        frameData.materialData.clear();
        frameData.materialData.resize(frameData.materialData.capacity());
    }
    {
        frameData.geometryData.clear();
        frameData.geometryData.resize(frameData.geometryData.capacity());
    }
    {
        frameData.instanceData.clear();
        frameData.instanceData.resize(frameData.instanceData.capacity());
    }
    {
        frameData.instances.clear();
        frameData.instances.resize(frameData.instances.capacity());
    }
    {
        frameData.directionalLightData.clear();
        frameData.directionalLightData.resize(frameData.directionalLightData.capacity());
    }
}

void HRay::EndScene(RendererData& data, FrameData& frameData, nvrhi::ICommandList* commandList, const ViewDesc& viewDesc)
{
    HE_PROFILE_FUNCTION();

    // SceneInfo
    {
        frameData.sceneInfo.view.worldToView = viewDesc.view;
        frameData.sceneInfo.view.viewToClip = viewDesc.projection;
        frameData.sceneInfo.view.clipToWorld = Math::inverse(viewDesc.projection * viewDesc.view);
        frameData.sceneInfo.view.cameraPosition = viewDesc.cameraPosition;
        frameData.sceneInfo.view.viewSize = Math::float2(viewDesc.width, viewDesc.height);
        frameData.sceneInfo.view.fov = Math::radians(viewDesc.fov);
        frameData.sceneInfo.view.viewSizeInv = 1.0f / frameData.sceneInfo.view.viewSize;
        frameData.sceneInfo.view.frameIndex = frameData.frameIndex;

        commandList->writeBuffer(frameData.sceneInfoBuffer, &frameData.sceneInfo, sizeof(SceneInfo));
    }

    if ((viewDesc.width > 0 && viewDesc.height > 0) && (viewDesc.width != frameData.renderTarget->getDesc().width || viewDesc.height != frameData.renderTarget->getDesc().height))
        CreateOrResizeRenderTarget(data, frameData, viewDesc.width, viewDesc.height);

    if (!frameData.bindingSet)
    {
        {
            HE_PROFILE_SCOPE("CreateBindingSet");

            HE_ASSERT(frameData.topLevelAS);
            HE_ASSERT(frameData.instanceBuffer);
            HE_ASSERT(frameData.geometryBuffer);
            HE_ASSERT(frameData.materialBuffer);
            HE_ASSERT(frameData.directionalLightBuffer);
            HE_ASSERT(frameData.renderTarget);
            HE_ASSERT(frameData.prevRenderTarget);
            HE_ASSERT(frameData.sceneInfoBuffer);

            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.bindings = {
                nvrhi::BindingSetItem::RayTracingAccelStruct(0, frameData.topLevelAS),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(1, frameData.instanceBuffer),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(2, frameData.geometryBuffer),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(3, frameData.materialBuffer),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(4, frameData.directionalLightBuffer),
                nvrhi::BindingSetItem::Texture_UAV(0, frameData.renderTarget),
                nvrhi::BindingSetItem::Texture_UAV(1, frameData.prevRenderTarget),
                nvrhi::BindingSetItem::Sampler(0, data.anisotropicWrapSampler),
                nvrhi::BindingSetItem::ConstantBuffer(0, frameData.sceneInfoBuffer)
            };

            frameData.bindingSet = data.device->createBindingSet(bindingSetDesc, data.bindingLayout);
        }

        {
            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.bindings = {
                nvrhi::BindingSetItem::Texture_UAV(0, frameData.renderTarget),
                //nvrhi::BindingSetItem::PushConstants(0, sizeof(uint32_t))
            };

            data.postProcessingBindingSet = data.device->createBindingSet(bindingSetDesc, data.postProcessingBindingLayout);
        }
    }

    commandList->writeBuffer(frameData.geometryBuffer, frameData.geometryData.data(), frameData.geometryCount * sizeof(GeometryData));
    commandList->writeBuffer(frameData.instanceBuffer, frameData.instanceData.data(), frameData.instanceCount * sizeof(InstanceData));
    commandList->writeBuffer(frameData.materialBuffer, frameData.materialData.data(), frameData.materialCount * sizeof(MaterialData));
    commandList->writeBuffer(frameData.directionalLightBuffer, frameData.directionalLightData.data(), frameData.sceneInfo.light.directionalLightCount * sizeof(DirectionalLightData));
    commandList->buildTopLevelAccelStruct(frameData.topLevelAS, frameData.instances.data(), frameData.instanceCount, nvrhi::rt::AccelStructBuildFlags::AllowEmptyInstances);

    nvrhi::rt::State state;
    state.shaderTable = data.shaderTable;
    state.bindings = { frameData.bindingSet, data.descriptorTable->GetDescriptorTable() };
    commandList->setRayTracingState(state);

    commandList->copyTexture(frameData.prevRenderTarget, {}, frameData.renderTarget, {});

    nvrhi::rt::DispatchRaysArguments args;
    args.width = viewDesc.width;
    args.height = viewDesc.height;
    commandList->dispatchRays(args);

    if (frameData.enablePostProcessing)
    {
        commandList->setComputeState({ data.computePipeline , { data.postProcessingBindingSet } });
        commandList->dispatch(viewDesc.width / 8, viewDesc.height / 8);
    }
    
    frameData.frameIndex += frameData.sceneInfo.settings.maxSamples;
    frameData.time = HE::Application::GetTime() - frameData.lastTime;
}

void HRay::SubmitMesh(RendererData& data, FrameData& frameData, Assets::Asset asset, Assets::Mesh& mesh, Math::float4x4 wt, nvrhi::ICommandList* cl)
{
    auto meshSource = mesh.meshSource;

    if (!asset.Has<MeshSourceBuffers, MeshSourceDescriptors>())
    {
        asset.Add<MeshSourceBuffers>();
        asset.Add<MeshSourceDescriptors>();
    }

    auto& buffers = asset.Get<MeshSourceBuffers>();
    auto& descriptors = asset.Get<MeshSourceDescriptors>();

    auto& indexBuffer = buffers.indexBuffer;
    auto& vertexBuffer = buffers.vertexBuffer;
    auto& indexBufferDescriptor = descriptors.indexBufferDescriptor;
    auto& vertexBufferDescriptor = descriptors.vertexBufferDescriptor;

    // indexBuffer
    if (!indexBuffer)
    {
        HE_PROFILE_SCOPE("Create IndexBuffer");

        nvrhi::BufferDesc bufferDesc;
        bufferDesc.isIndexBuffer = true;
        bufferDesc.byteSize = meshSource->cpuIndexBuffer.size() * sizeof(uint32_t);
        bufferDesc.debugName = "IndexBuffer";
        bufferDesc.canHaveTypedViews = true;
        bufferDesc.canHaveRawViews = true;
        bufferDesc.format = nvrhi::Format::R32_UINT;
        bufferDesc.isAccelStructBuildInput = true;
        indexBuffer = data.device->createBuffer(bufferDesc);

        indexBufferDescriptor = data.descriptorTable->CreateDescriptorHandle(nvrhi::BindingSetItem::RawBuffer_SRV(0, indexBuffer));
        cl->beginTrackingBufferState(indexBuffer, nvrhi::ResourceStates::Common);
        cl->writeBuffer(indexBuffer, meshSource->cpuIndexBuffer.data(), meshSource->cpuIndexBuffer.size() * sizeof(uint32_t));

        nvrhi::ResourceStates state = nvrhi::ResourceStates::IndexBuffer | nvrhi::ResourceStates::ShaderResource | nvrhi::ResourceStates::AccelStructBuildInput;

        cl->setPermanentBufferState(indexBuffer, state);
        cl->commitBarriers();
    }

    // vertexBuffer
    if (!vertexBuffer)
    {
        HE_PROFILE_SCOPE("Create VertexBuffer");

        nvrhi::BufferDesc bufferDesc;
        bufferDesc.isVertexBuffer = true;
        bufferDesc.debugName = "VertexBuffer";
        bufferDesc.canHaveTypedViews = true;
        bufferDesc.canHaveRawViews = true;
        bufferDesc.isAccelStructBuildInput = true;
        bufferDesc.byteSize = meshSource->cpuVertexBuffer.size();
        vertexBuffer = data.device->createBuffer(bufferDesc);

        vertexBufferDescriptor = data.descriptorTable->CreateDescriptorHandle(nvrhi::BindingSetItem::RawBuffer_SRV(0, vertexBuffer));
        cl->beginTrackingBufferState(vertexBuffer, nvrhi::ResourceStates::Common);
        if (meshSource->HasAttribute(Assets::VertexAttribute::Position))
        {
            const auto& range = meshSource->getVertexBufferRange(Assets::VertexAttribute::Position);
            cl->writeBuffer(vertexBuffer, meshSource->GetAttribute<Math::float3>(Assets::VertexAttribute::Position), range.byteSize, range.byteOffset);
        }

        if (meshSource->HasAttribute(Assets::VertexAttribute::Normal))
        {
            const auto& range = meshSource->getVertexBufferRange(Assets::VertexAttribute::Normal);
            cl->writeBuffer(vertexBuffer, meshSource->GetAttribute<uint32_t>(Assets::VertexAttribute::Normal), range.byteSize, range.byteOffset);
        }

        if (meshSource->HasAttribute(Assets::VertexAttribute::Tangent))
        {
            const auto& range = meshSource->getVertexBufferRange(Assets::VertexAttribute::Tangent);
            cl->writeBuffer(vertexBuffer, meshSource->GetAttribute<uint32_t>(Assets::VertexAttribute::Tangent), range.byteSize, range.byteOffset);
        }

        if (meshSource->HasAttribute(Assets::VertexAttribute::TexCoord0))
        {
            const auto& range = meshSource->getVertexBufferRange(Assets::VertexAttribute::TexCoord0);
            cl->writeBuffer(vertexBuffer, meshSource->GetAttribute<Math::float2>(Assets::VertexAttribute::TexCoord0), range.byteSize, range.byteOffset);
        }

        if (meshSource->HasAttribute(Assets::VertexAttribute::TexCoord1))
        {
            const auto& range = meshSource->getVertexBufferRange(Assets::VertexAttribute::TexCoord1);
            cl->writeBuffer(vertexBuffer, meshSource->GetAttribute<Math::float2>(Assets::VertexAttribute::TexCoord1), range.byteSize, range.byteOffset);
        }

        nvrhi::ResourceStates state = nvrhi::ResourceStates::VertexBuffer | nvrhi::ResourceStates::ShaderResource | nvrhi::ResourceStates::AccelStructBuildInput;

        cl->setPermanentBufferState(vertexBuffer, state);
        cl->commitBarriers();
    }

    // BLAS
    if (!mesh.accelStruct)
    {
        HE_PROFILE_SCOPE("Create BLAS");

        nvrhi::rt::AccelStructDesc blasDesc;
        blasDesc.isTopLevel = false;

        blasDesc.bottomLevelGeometries.reserve(mesh.GetGeometrySpan().size());
        for (auto& geometry : mesh.GetGeometrySpan())
        {
            nvrhi::rt::GeometryDesc& geometryDesc = blasDesc.bottomLevelGeometries.emplace_back();
            auto& triangles = geometryDesc.geometryData.triangles;
            triangles.indexBuffer = indexBuffer;
            triangles.indexOffset = geometry.GetIndexRange().byteOffset;
            triangles.indexFormat = nvrhi::Format::R32_UINT;
            triangles.indexCount = geometry.indexCount;
            triangles.vertexBuffer = vertexBuffer;
            triangles.vertexOffset = geometry.GetVertexRange(Assets::VertexAttribute::Position).byteOffset;
            triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
            triangles.vertexStride = Assets::GetVertexAttributeSize(Assets::VertexAttribute::Position);
            triangles.vertexCount = geometry.vertexCount;
            geometryDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
            geometryDesc.flags = nvrhi::rt::GeometryFlags::Opaque;
        }

        mesh.accelStruct = data.device->createAccelStruct(blasDesc);
        nvrhi::utils::BuildBottomLevelAccelStruct(cl, mesh.accelStruct, blasDesc);
    }

    if (frameData.instanceData.size() <= frameData.instanceCount)
        CreateOrResizeInstanceBuffer(data, frameData, (uint32_t)frameData.instanceData.size() * 2);

    // TLAS
    {
        nvrhi::rt::InstanceDesc& instanceDesc = frameData.instances[frameData.instanceCount];

        HE_ASSERT(mesh.accelStruct);
        instanceDesc.bottomLevelAS = mesh.accelStruct;
        instanceDesc.instanceMask = c_instanceMaskOpaque;
        instanceDesc.instanceID = frameData.instanceCount;
        //instanceDesc.instanceContributionToHitGroupIndex = ?; // TODO : What is it?

        Math::float3x4 transform = Math::transpose(wt);
        std::memcpy(instanceDesc.transform, &transform, sizeof(transform));
    }

    // InstanceData
    {
        InstanceData& idata = frameData.instanceData[frameData.instanceCount];
        idata.transform = wt;
        idata.firstGeometryIndex = frameData.geometryCount;
    }

    // Geometry
    {
        for (const auto& geometry : mesh.GetGeometrySpan())
        {
            if (frameData.geometryData.size() <= frameData.geometryCount)
                CreateOrResizeGeoBuffer(data, frameData,(uint32_t)frameData.geometryData.size() * 2);

            if (frameData.materialData.size() <= frameData.materialCount)
                CreateOrResizeMaterialBuffer(data, frameData,(uint32_t)frameData.materialData.size() * 2);

            GeometryData& gd = frameData.geometryData[frameData.geometryCount];
            gd.indexBufferIndex = indexBufferDescriptor.IsValid() ? indexBufferDescriptor.Get() : c_Invalid;
            gd.vertexBufferIndex = vertexBufferDescriptor.IsValid() ? vertexBufferDescriptor.Get() : c_Invalid;
            gd.indexCount = geometry.indexCount;
            gd.vertexCount = geometry.vertexCount;
            gd.indexOffset = (uint32_t)geometry.GetIndexRange().byteOffset;
            gd.positionOffset = meshSource->HasAttribute(Assets::VertexAttribute::Position) ? (uint32_t)geometry.GetVertexRange(Assets::VertexAttribute::Position).byteOffset : c_Invalid;
            gd.normalOffset = meshSource->HasAttribute(Assets::VertexAttribute::Normal) ? (uint32_t)geometry.GetVertexRange(Assets::VertexAttribute::Normal).byteOffset : c_Invalid;
            gd.tangentOffset = meshSource->HasAttribute(Assets::VertexAttribute::Tangent) ? (uint32_t)geometry.GetVertexRange(Assets::VertexAttribute::Tangent).byteOffset : c_Invalid;
            gd.texCoord0Offset = meshSource->HasAttribute(Assets::VertexAttribute::TexCoord0) ? (uint32_t)geometry.GetVertexRange(Assets::VertexAttribute::TexCoord0).byteOffset : c_Invalid;
            gd.texCoord1Offset = meshSource->HasAttribute(Assets::VertexAttribute::TexCoord1) ? (uint32_t)geometry.GetVertexRange(Assets::VertexAttribute::TexCoord1).byteOffset : c_Invalid;

            // materials
            {
                Assets::Material* material = data.am->GetAsset<Assets::Material>(geometry.materailHandle);
                HE_ASSERT(material);

                if (frameData.materials.contains(geometry.materailHandle))
                {
                    uint32_t index = frameData.materials.at(geometry.materailHandle);
                    gd.materialIndex = index;
                }
                else
                {
                    gd.materialIndex = c_DefaultMaterialIndex;
                }
            }

            frameData.geometryCount++;
        }
    }

    frameData.instanceCount++;
}

void HRay::SubmitMaterial(RendererData& data, FrameData& frameData, Assets::Asset materailAsset)
{
    if (!materailAsset)
        return;

    auto& material = materailAsset.Get<Assets::Material>();
    auto handle = materailAsset.GetHandle();

    if (!frameData.materials.contains(handle))
    {
        frameData.materials[handle] = frameData.materialCount;
        frameData.materialCount++;
    }

    Assets::Texture* baseTexture = data.am->GetAsset<Assets::Texture>(material.baseTextureHandle);
    if (baseTexture && baseTexture->texture && !baseTexture->descriptor.IsValid())
    {
        HE_ASSERT(baseTexture->texture);
        baseTexture->descriptor = data.descriptorTable->CreateDescriptorHandle(nvrhi::BindingSetItem::Texture_SRV(0, baseTexture->texture));
        data.textureCount++;
    }

    Assets::Texture* emissiveTexture = data.am->GetAsset<Assets::Texture>(material.emissiveTextureHandle);
    if (emissiveTexture && emissiveTexture->texture && !emissiveTexture->descriptor.IsValid())
    {
        HE_ASSERT(emissiveTexture->texture);
        emissiveTexture->descriptor = data.descriptorTable->CreateDescriptorHandle(nvrhi::BindingSetItem::Texture_SRV(0, emissiveTexture->texture));
        data.textureCount++;
    }

    Assets::Texture* metallicRoughnessTexture = data.am->GetAsset<Assets::Texture>(material.metallicRoughnessTextureHandle);
    if (metallicRoughnessTexture && metallicRoughnessTexture->texture && !metallicRoughnessTexture->descriptor.IsValid())
    {
        HE_ASSERT(metallicRoughnessTexture->texture);
        metallicRoughnessTexture->descriptor = data.descriptorTable->CreateDescriptorHandle(nvrhi::BindingSetItem::Texture_SRV(0, metallicRoughnessTexture->texture));
        data.textureCount++;
    }

    Assets::Texture* normalTexture = data.am->GetAsset<Assets::Texture>(material.normalTextureHandle);
    if (normalTexture && normalTexture->texture && !normalTexture->descriptor.IsValid())
    {
        HE_ASSERT(normalTexture->texture);
        normalTexture->descriptor = data.descriptorTable->CreateDescriptorHandle(nvrhi::BindingSetItem::Texture_SRV(0, normalTexture->texture));
        data.textureCount++;
    }

    uint32_t index = frameData.materials.at(handle);
    auto& mat = frameData.materialData[index];
    mat.baseColor = material.baseColor;
    mat.metallic = material.metallic;
    mat.roughness = material.roughness;
    mat.uvSet = (int)material.uvSet;
    mat.emissiveColor = Math::convertSRGBToLinear(material.emissiveColor) * material.emissiveEV;
    mat.baseTextureIndex = baseTexture ? baseTexture->descriptor.Get() : c_Invalid;
    mat.emissiveTextureIndex = emissiveTexture ? emissiveTexture->descriptor.Get() : c_Invalid;
    mat.metallicRoughnessTextureIndex = metallicRoughnessTexture ? metallicRoughnessTexture->descriptor.Get() : c_Invalid;
    mat.normalTextureIndex = normalTexture ? normalTexture->descriptor.Get() : c_Invalid;
    mat.uvMat = Math::CreateMat3(material.offset, material.rotation, material.scale);
}

void HRay::SubmitDirectionalLight(RendererData& data, FrameData& frameData, const Assets::DirectionalLightComponent& light, Math::float4x4 wt)
{
    if (frameData.directionalLightData.size() <= frameData.sceneInfo.light.directionalLightCount)
        CreateOrResizeDirectionalLightBuffer(data, frameData, (uint32_t)frameData.directionalLightData.size() * 2);

    HRay::DirectionalLightData& l = frameData.directionalLightData[frameData.sceneInfo.light.directionalLightCount];
    l.color = light.color;
    l.intensity = light.intensity;
    l.angularRadius = light.angularRadius;
    l.haloSize = light.haloSize;
    l.haloFalloff = light.haloFalloff;
    l.direction = glm::normalize(glm::vec3(wt[2]));

    frameData.sceneInfo.light.directionalLightCount++;
}

void HRay::SubmitSkyLight(RendererData& data, FrameData& frameData, const Assets::DynamicSkyLightComponent& light)
{
    frameData.sceneInfo.light.groundColor = Math::float4(light.groundColor,1);
    frameData.sceneInfo.light.horizonSkyColor = Math::float4(light.horizonSkyColor, 1);
    frameData.sceneInfo.light.zenithSkyColor = Math::float4(light.zenithSkyColor, 1);
}

void HRay::Clear(FrameData& frameData)
{
    frameData.frameIndex = 0;
    frameData.lastTime = HE::Application::GetTime();
}

void HRay::ReleaseTexture(RendererData& data,  Assets::Texture* texture)
{
    if (texture->descriptor.IsValid())
    {
        data.descriptorTable->ReleaseDescriptor(texture->descriptor.Get());
        texture->descriptor.Reset();
        data.textureCount--;
    }
}
