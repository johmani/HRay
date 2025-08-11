#include <HydraEngine/Base.h>

#if NVRHI_HAS_D3D12
#include "Embeded/dxil/Main.bin.h"
#endif

#if NVRHI_HAS_VULKAN
#include "Embeded/spirv/Main.bin.h"
#endif

import HRay;
import nvrhi;
import HE;
import Assets;
import Math;
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

struct HitInfo
{
    Math::float3 normal;
    Math::float3 ffnormal;
    Math::float3 tangent;
    Math::float3 bitangent;
    float distance;
    uint32_t entityID;

    Math::float3 baseColor;
    Math::float3 emissive;
    float metallic;
    float roughness;
    float anisotropic;
    float subsurface;
    float specularTint;
    float sheen;
    float sheenTint;
    float clearcoat;
    float clearcoatRoughness;
    float specTrans;
    float ior;

    float eta;
    float ax, ay;
};

static void GetCameraBasis(const Math::float4x4& clipToWorld, Math::float3& camFront, Math::float3& camUp, Math::float3& camRight)
{
    Math::float4 originCS = Math::float4(0, 0, 0, 1);
    Math::float4 rightCS = Math::float4(1, 0, 0, 1);
    Math::float4 upCS = Math::float4(0, 1, 0, 1);

    Math::float3 worldOrigin = Math::float3(clipToWorld * originCS);
    Math::float3 worldRight = Math::float3(clipToWorld * rightCS);
    Math::float3 worldUp = Math::float3(clipToWorld * upCS);

    camRight = Math::normalize(worldRight - worldOrigin);
    camUp = Math::normalize(worldUp - worldOrigin);
    camFront = Math::normalize(Math::cross(camUp, camRight));
}

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

static float Luminance(float r, float g, float b)
{
    return 0.212671f * r + 0.715160f * g + 0.072169f * b;
}

static void CreateOrResizeRenderTarget(HRay::RendererData& data, HRay::FrameData& frameData, uint32_t width, uint32_t height)
{
    HE_PROFILE_FUNCTION();

    nvrhi::TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    desc.format = nvrhi::Format::RGBA32_FLOAT;
    desc.keepInitialState = true;
    desc.isRenderTarget = false;
    desc.isUAV = true;

    desc.debugName = "HDRColor";
    frameData.HDRColor = data.device->createTexture(desc);

    desc.debugName = "accumulationOutput";
    frameData.accumulationOutput = data.device->createTexture(desc);

    desc.format = nvrhi::Format::RGBA16_UNORM;
    desc.debugName = "LDRColor";
    frameData.LDRColor = data.device->createTexture(desc);

    desc.format = nvrhi::Format::D32;
    desc.debugName = "Depth";
    frameData.depth = data.device->createTexture(desc);

    desc.format = nvrhi::Format::R32_UINT;
    desc.debugName = "entitiesID";
    frameData.entitiesID = data.device->createTexture(desc);

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
        {
            HE_PROFILE_SCOPE("CreateShaderLibrary");

            data.shaderLibrary = HE::RHI::CreateShaderLibrary(data.device, STATIC_SHADER(Main), nullptr);
            HE_VERIFY(data.shaderLibrary);
        }
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
           nvrhi::BindingLayoutItem::Texture_UAV(2),
           nvrhi::BindingLayoutItem::Texture_UAV(3),
           nvrhi::BindingLayoutItem::Texture_UAV(4),
           nvrhi::BindingLayoutItem::Sampler(0),
           nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)
        };

        data.bindingLayout = data.device->createBindingLayout(desc);
        HE_ASSERT(data.bindingLayout);
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
            data.shaderLibrary->getShader("AnyHit", nvrhi::ShaderType::AnyHit),
            nullptr,
            nullptr,
            false
        } };

        pipelineDesc.maxPayloadSize = sizeof(HitInfo);
        data.pipeline = data.device->createRayTracingPipeline(pipelineDesc);
        HE_VERIFY(data.pipeline);

        data.shaderTable = data.pipeline->createShaderTable();
        data.shaderTable->setRayGenerationShader("RayGen");
        data.shaderTable->addMissShader("Miss");
        data.shaderTable->addHitGroup("HitGroup");
    }
}

void HRay::BeginScene(RendererData& data, FrameData& frameData)
{
    HE_PROFILE_FUNCTION();

    if (!frameData.HDRColor)
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
        float fov = Math::radians(viewDesc.fov);
        float aspect = (float)viewDesc.width / (float)viewDesc.height;
        float halfHeight = tan(fov * 0.5f) * frameData.sceneInfo.view.focusDistance;
        float halfWidth = halfHeight * aspect;

        frameData.sceneInfo.view.worldToView = viewDesc.view;
        frameData.sceneInfo.view.viewToClip = viewDesc.projection;
        frameData.sceneInfo.view.clipToWorld = Math::inverse(viewDesc.projection * viewDesc.view);
        frameData.sceneInfo.view.cameraPosition = viewDesc.cameraPosition;
        GetCameraBasis(frameData.sceneInfo.view.clipToWorld, frameData.sceneInfo.view.front, frameData.sceneInfo.view.up, frameData.sceneInfo.view.right);
        frameData.sceneInfo.view.viewSize = Math::float2(viewDesc.width, viewDesc.height);
        frameData.sceneInfo.view.viewSizeInv = 1.0f / frameData.sceneInfo.view.viewSize;
        frameData.sceneInfo.view.frameIndex = frameData.frameIndex;
        frameData.sceneInfo.view.halfWidth = halfWidth;
        frameData.sceneInfo.view.halfHeight = halfHeight;
        frameData.sceneInfo.view.focalCenter = viewDesc.cameraPosition + frameData.sceneInfo.view.front * frameData.sceneInfo.view.focusDistance;
        frameData.sceneInfo.view.fov = fov;

        commandList->writeBuffer(frameData.sceneInfoBuffer, &frameData.sceneInfo, sizeof(SceneInfo));
    }

    if ((viewDesc.width > 0 && viewDesc.height > 0) && (viewDesc.width != frameData.HDRColor->getDesc().width || viewDesc.height != frameData.HDRColor->getDesc().height))
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
            HE_ASSERT(frameData.HDRColor);
            HE_ASSERT(frameData.accumulationOutput);
            HE_ASSERT(frameData.LDRColor);
            HE_ASSERT(frameData.depth);
            HE_ASSERT(frameData.entitiesID);
            HE_ASSERT(frameData.sceneInfoBuffer);

            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.bindings = {
                nvrhi::BindingSetItem::RayTracingAccelStruct(0, frameData.topLevelAS),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(1, frameData.instanceBuffer),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(2, frameData.geometryBuffer),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(3, frameData.materialBuffer),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(4, frameData.directionalLightBuffer),
                nvrhi::BindingSetItem::Texture_UAV(0, frameData.HDRColor),
                nvrhi::BindingSetItem::Texture_UAV(1, frameData.accumulationOutput),
                nvrhi::BindingSetItem::Texture_UAV(2, frameData.LDRColor),
                nvrhi::BindingSetItem::Texture_UAV(3, frameData.depth),
                nvrhi::BindingSetItem::Texture_UAV(4, frameData.entitiesID),
                nvrhi::BindingSetItem::Sampler(0, data.anisotropicWrapSampler),
                nvrhi::BindingSetItem::ConstantBuffer(0, frameData.sceneInfoBuffer),
            };

            frameData.bindingSet = data.device->createBindingSet(bindingSetDesc, data.bindingLayout);
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

    commandList->copyTexture(frameData.accumulationOutput, {}, frameData.HDRColor, {});

    nvrhi::rt::DispatchRaysArguments args;
    args.width = viewDesc.width;
    args.height = viewDesc.height;
    commandList->dispatchRays(args);

    frameData.frameIndex += frameData.sceneInfo.settings.maxSamples;
    frameData.time = HE::Application::GetTime() - frameData.lastTime;
}

void HRay::SubmitMesh(RendererData& data, FrameData& frameData, Assets::Asset asset, Assets::Mesh& mesh, Math::float4x4 wt, uint32_t id, nvrhi::ICommandList* cl)
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

            switch (geometry.alfaMode)
            {
            case Assets::AlfaMode::Opaque: geometryDesc.flags = nvrhi::rt::GeometryFlags::Opaque; break;
            case Assets::AlfaMode::Blend: geometryDesc.flags = nvrhi::rt::GeometryFlags::None; break;
            case Assets::AlfaMode::Mask: geometryDesc.flags = nvrhi::rt::GeometryFlags::None; break;
            }
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
        idata.id = id;
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
            if (frameData.materials.contains(geometry.materailHandle))
            {
                uint32_t index = frameData.materials.at(geometry.materailHandle);
                gd.materialIndex = index;
            }
            else
            {
                gd.materialIndex = c_DefaultMaterialIndex;
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

    mat.baseColor           = material.baseColor;
    mat.metallic            = material.metallic;
    mat.roughness           = material.roughness;
    mat.emissiveColor       = material.emissiveColor * material.emissiveEV;
    mat.anisotropic         = material.anisotropic;
    mat.subsurface          = material.subsurface;       
    mat.specularTint        = material.specularTint;      
    mat.sheen               = material.sheen;
    mat.sheenTint           = material.sheenTint;
    mat.clearcoat           = material.clearcoat;
    mat.clearcoatRoughness  = material.clearcoatRoughness;
    mat.transmission        = material.transmission;
    mat.ior                 = material.ior;

    mat.alfaMode            = (HRay::AlfaMode)material.alfaMode;
    mat.alphaCutoff         = material.alphaCutoff;
    
    mat.uvSet = (int)material.uvSet;
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

void HRay::SubmitSkyLight(RendererData& data, FrameData& frameData, Assets::SkyLightComponent& light, float rotation)
{
    frameData.sceneInfo.light.groundColor = Math::float4((light.groundColor), 1);
    frameData.sceneInfo.light.horizonSkyColor = Math::float4((light.horizonSkyColor), 1);
    frameData.sceneInfo.light.zenithSkyColor = Math::float4((light.zenithSkyColor), 1);
    frameData.sceneInfo.light.rotation = rotation;
    frameData.sceneInfo.light.intensity = light.intensity;
    frameData.sceneInfo.light.descriptorIndex = c_Invalid;

    auto asset = data.am->GetAsset(light.textureHandle);
    if (!asset || asset.GetState() != Assets::AssetState::Loaded)
        return;

    Assets::Texture* hdr = data.am->GetAsset<Assets::Texture>(light.textureHandle);

    auto width = hdr->texture->getDesc().width;
    auto height = hdr->texture->getDesc().height;
    frameData.sceneInfo.light.size = { width, height };

    if (hdr && hdr->texture && !hdr->descriptor.IsValid())
    {
        hdr->descriptor = data.descriptorTable->CreateDescriptorHandle(nvrhi::BindingSetItem::Texture_SRV(0, hdr->texture));
        data.textureCount++;
    }

    if (light.totalSum == -1)
    {
        float* weights = new float[width * height];

        const auto& desc = hdr->texture->getDesc();
        auto commandList = data.device->createCommandList({ .enableImmediateExecution = false });
        commandList->open();
        nvrhi::StagingTextureHandle stagingTexture = data.device->createStagingTexture(desc, nvrhi::CpuAccessMode::Read);
        HE_VERIFY(stagingTexture);
        commandList->copyTexture(stagingTexture, nvrhi::TextureSlice(), hdr->texture, nvrhi::TextureSlice());
        commandList->close();
        data.device->executeCommandList(commandList);

        size_t rowPitch = 0;
        void* pData = data.device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch);
        HE_VERIFY(pData);

        for (uint32_t y = 0; y < height; ++y)
        {
            const float* row = reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(pData) + y * rowPitch);

            for (uint32_t x = 0; x < width; ++x)
            {
                float r = row[x * 3 + 0];
                float g = row[x * 3 + 1];
                float b = row[x * 3 + 2];

                weights[x + y * width] = Luminance(r, g, b);
            }
        }

        data.device->unmapStagingTexture(stagingTexture);

        // Build CDF
        float* cdf = new float[width * height];
        cdf[0] = weights[0];
        for (uint32_t i = 1; i < width * height; i++)
            cdf[i] = cdf[i - 1] + weights[i];

        float sum = cdf[width * height - 1];
        light.totalSum = sum;

        delete[] weights;
        delete[] cdf;
    }

    frameData.sceneInfo.light.totalSum = light.totalSum;
    frameData.sceneInfo.light.descriptorIndex = hdr ? hdr->descriptor.Get() : c_Invalid;
}

void HRay::Clear(FrameData& frameData)
{
    frameData.frameIndex = 0;
    frameData.lastTime = HE::Application::GetTime();
}

nvrhi::ITexture* HRay::GetColorTarget(FrameData& frameData)
{
    return frameData.LDRColor;
}

nvrhi::ITexture* HRay::GetDepthTarget(FrameData& frameData)
{
    return frameData.depth;
}

nvrhi::ITexture* HRay::GetEntitiesIDTarget(FrameData& frameData)
{
    return frameData.entitiesID;
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
