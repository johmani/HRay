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

    struct SceneInfo
    {
        // View
        Math::float4x4 worldToView;
        Math::float4x4 viewToClip;
        Math::float4x4 clipToWorld;

        Math::float3 cameraPosition;
        uint32_t frameIndex;

        Math::float2 viewSize;
        Math::float2 viewSizeInv;

        float minDistance = 0.001f;
        float maxDistance = 1000.0f;
        float divergeStrength = 1.0f;
        float defocusStrength = 0.0f;
        
        float focusDist = 10.0f;
        float fov = 60.0f;
        Math::float2 padding0;

        // Light
        Math::float4 groundColour = { 0.35, 0.3, 0.35, 1 };
        Math::float4 skyColourHorizon = { 1, 1, 1, 1 };
        Math::float4 skyColourZenith = { 0.0788092, 0.36480793, 0.7264151, 1 };

        // Settings
        int directionalLightCount;
        int maxLighteBounces = 8;
        Math::float2 padding1;

        int maxSamples = 1;
        float gamma = 2.2f;
        Math::float2 padding2;

        bool enableEnvironmentLight = true;  // 4 bytes
        bool padding3[3];
        bool enableVisualFocusDist = false;   // new 16-byte block start
        bool padding4[3];
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
        float metallic;								  // Range(0, 1)
        float roughness;							  // Range(0, 1)
        int uvSet;
        int padding0;

        Math::float3 emissiveColor;

        uint32_t baseTextureIndex;
        uint32_t emissiveTextureIndex;
        uint32_t metallicRoughnessTextureIndex;
        uint32_t normalTextureIndex;

        Math::float3x3 uvMat;
    };

    struct DirectionalLightData
    {
        Math::float3 direction;
        Math::float3 color;
        float intensity;
        float angularRadius;
        float haloSize;
        float haloFalloff;
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
        nvrhi::BufferHandle sceneInfoBuffer;
        nvrhi::rt::ShaderTableHandle shaderTable;
        nvrhi::rt::AccelStructHandle topLevelAS;
        Assets::Material defultMaterial;
        HE::Ref<Assets::DescriptorTableManager> descriptorTable;
        nvrhi::BindingLayoutHandle bindlessLayout;
        nvrhi::TextureHandle prevRenderTarget;
        nvrhi::BindingLayoutHandle postProcessingBindingLayout;
        nvrhi::BindingSetHandle postProcessingBindingSet;
        nvrhi::ComputePipelineHandle computePipeline;
        nvrhi::ShaderHandle cs;

        // Frame Data
        nvrhi::BufferHandle instanceBuffer;
        nvrhi::BufferHandle geometryBuffer;
        nvrhi::BufferHandle materialBuffer;
        nvrhi::BufferHandle directionalLightBuffer;
        std::vector<nvrhi::rt::InstanceDesc> instances;
        std::vector<InstanceData> instanceData;
        std::vector<GeometryData> geometryData;
        std::vector<MaterialData> materialData;
        std::vector<DirectionalLightData> directionalLightData;
        std::map<Assets::AssetHandle, uint32_t> materials;
        SceneInfo sceneInfo;
        uint32_t frameIndex = 0;
        float time = 0.0f;
        float lastTime = 0.0f;
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
        float fov;
        uint32_t width, height;
    };

    void Init(RendererData& data, nvrhi::DeviceHandle pDevice, nvrhi::CommandListHandle commandList);
    void BeginScene(RendererData& data);
    void EndScene(RendererData& data, nvrhi::ICommandList* commandList, const ViewDesc& viewDesc);
    void SubmitMesh(RendererData& data, Assets::Asset asset, Assets::Mesh& mesh, Math::float4x4 wt, nvrhi::ICommandList* cl);
    void SubmitDirectionalLight(RendererData& data, const Assets::DirectionalLightComponent& light, Math::float4x4 wt, nvrhi::ICommandList* cl);
    void ReleaseTexture(RendererData& data, Assets::Texture* texture);
    void Clear(RendererData& data);
}