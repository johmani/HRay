module;

#include <HydraEngine/Base.h>

export module HRay;

import HE;
import Math;
import nvrhi;
import Assets;
import std;

export namespace HRay {

    constexpr uint32_t c_instanceMaskOpaque = 1;
    constexpr uint32_t c_Invalid = ~0u;

    struct ViewBuffer
    {
        Math::float4x4 clipToWorld;

        Math::float3 cameraPosition;
        int padding;

        Math::float2 viewSize;
        Math::float2 viewSizeInv;
    };

    struct GeometryData
    {
        Math::uint indexBufferIndex;
        Math::uint vertexBufferIndex;

        Math::uint indexCount;
        Math::uint vertexCount;

        Math::uint indexOffset;
        Math::uint positionOffset;
        Math::uint normalOffset;
        Math::uint tangentOffset;
        Math::uint texCoord0Offset;
        Math::uint texCoord1Offset;
        Math::uint materialIndex = c_Invalid;
    };

    struct InstanceData
    {
        Math::uint firstGeometryIndex;
        Math::float4x4 transform;
    };

    struct MaterialData
    {
        Math::float4 baseColor;
        uint32_t baseTextureIndex;

        int uvSet;
        int padding0;
        int padding1;
    };

    struct RendererData
    {
        Assets::AssetManager* am;

        nvrhi::DeviceHandle device;
        nvrhi::ShaderLibraryHandle shaderLibrary;
        nvrhi::BindingLayoutHandle bindingLayout;
        nvrhi::BindingSetHandle bindingSet;
        nvrhi::SamplerHandle anisotropicWrapSampler;
        nvrhi::rt::PipelineHandle pipeline;
        nvrhi::TextureHandle renderTarget;
        nvrhi::BufferHandle viewBuffer;
        nvrhi::rt::ShaderTableHandle shaderTable;
        nvrhi::rt::AccelStructHandle topLevelAS;
        Assets::Material defultMaterial;
        HE::Ref<Assets::DescriptorTableManager> descriptorTable;
        nvrhi::BindingLayoutHandle bindlessLayout;

        // Frame Data
        nvrhi::BufferHandle instanceBuffer;
        nvrhi::BufferHandle geometryBuffer;
        nvrhi::BufferHandle materialBuffer;
        std::vector<nvrhi::rt::InstanceDesc> instances;
        std::vector<InstanceData> instanceData;
        std::vector<GeometryData> geometryData;
        std::vector<MaterialData> materialData;
        std::map<Assets::AssetHandle, uint32_t> materials;

        uint32_t geometryCount = 0;
        uint32_t instanceCount = 0;
        uint32_t materialCount = 0;
        uint32_t textureCount = 0;
    };

    struct ViewDesc
    {
        Math::float4x4 view;
        Math::float4x4 projection;
        Math::float3 cameraPosition;
        uint32_t width, height;
    };

    void ReleaseTexture(RendererData& data, Assets::Texture* texture);
    void Init(RendererData& data, nvrhi::DeviceHandle pDevice, nvrhi::CommandListHandle commandList);
    void BeginScene(RendererData& data);
    void EndScene(RendererData& data, nvrhi::ICommandList* commandList, const ViewDesc& viewDesc);
    void SubmitMesh(RendererData& data, Assets::Asset asset, Assets::Mesh& mesh, Math::float4x4 wt, nvrhi::ICommandList* cl);
}