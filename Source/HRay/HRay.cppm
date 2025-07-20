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
        struct View
        {
            Math::float4x4 worldToView;
            Math::float4x4 viewToClip;
            Math::float4x4 clipToWorld;

            Math::float3 cameraPosition;
            uint32_t frameIndex;

            Math::float2 viewSize;
            Math::float2 viewSizeInv;

            float minDistance = 0.001f;
            float maxDistance = 1000.0f;
            float apertureRadius = 1.0f;
            float focusFalloff = 0.0f;

            float focusDistance = 10.0f;
            float fov = 60.0f;
            Math::float2 padding0;

            bool enableVisualFocusDistance = false;   // new 16-byte block start
            bool padding1[3];

            bool enableDepthOfField = false;   // new 16-byte block start
            bool padding2[3];
            Math::float2 padding3;

        } view;

        struct Light
        {
            Math::float4 groundColor;
            Math::float4 horizonSkyColor;
            Math::float4 zenithSkyColor;
            
            int directionalLightCount;
            bool enableEnvironmentLight = true;
            bool padding0[3];
            Math::float2 padding1;

        } light;
        
        struct Settings
        {
            int maxLighteBounces = 8;
            int maxSamples = 1;
            float gamma = 2.2f;
            int padding0;
           
        } settings;
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
        Math::float4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float metallic = 0.0f;
        float roughness = 0.5f;
        int uvSet = 0;
        int padding0;

        Math::float3 emissiveColor = {0.0f, 0.0f, 0.0f};

        uint32_t baseTextureIndex = c_Invalid;
        uint32_t emissiveTextureIndex = c_Invalid;
        uint32_t metallicRoughnessTextureIndex = c_Invalid;
        uint32_t normalTextureIndex = c_Invalid;

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
        nvrhi::SamplerHandle anisotropicWrapSampler;
        nvrhi::rt::PipelineHandle pipeline;
        nvrhi::rt::ShaderTableHandle shaderTable;
        HE::Ref<Assets::DescriptorTableManager> descriptorTable;
        nvrhi::BindingLayoutHandle bindlessLayout;
        nvrhi::BindingLayoutHandle postProcessingBindingLayout;
        nvrhi::BindingSetHandle postProcessingBindingSet;
        nvrhi::ComputePipelineHandle computePipeline;
        nvrhi::ShaderHandle cs;

        uint32_t textureCount = 0;
    };

    struct FrameData
    {
        nvrhi::BindingSetHandle bindingSet;
        nvrhi::BufferHandle sceneInfoBuffer;
        nvrhi::TextureHandle prevRenderTarget;
        nvrhi::TextureHandle renderTarget;
        nvrhi::BufferHandle instanceBuffer;
        nvrhi::BufferHandle geometryBuffer;
        nvrhi::BufferHandle materialBuffer;
        nvrhi::BufferHandle directionalLightBuffer;
        nvrhi::rt::AccelStructHandle topLevelAS;
       
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
        bool enablePostProcessing = false;
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
    void BeginScene(RendererData& data, FrameData& frameData);
    void EndScene(RendererData& data, FrameData& frameData, nvrhi::ICommandList* commandList, const ViewDesc& viewDesc);
    void SubmitMesh(RendererData& data, FrameData& frameData, Assets::Asset asset, Assets::Mesh& mesh, Math::float4x4 wt, nvrhi::ICommandList* cl);
    void SubmitMaterial(RendererData& data, FrameData& frameData, Assets::Asset materailAsset);
    void SubmitDirectionalLight(RendererData& data, FrameData& frameData, const Assets::DirectionalLightComponent& light, Math::float4x4 wt);
    void SubmitSkyLight(RendererData& data, FrameData& frameData, const Assets::DynamicSkyLightComponent& light);
    void ReleaseTexture(RendererData& data, Assets::Texture* texture);
    void Clear(FrameData& frameData);
}