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

    enum class TonMapingType : int
    {
        None,
        WhatEver,
        ACES,
        ACESFitted,
        Filmic,
        Reinhard
    };

    enum class RenderingMode : int
    {
        PathTracing,
        Normals,
        Tangent,
        Bitangent
    };

    enum class AlfaMode : int
    {
        Opaque,
        Mask,
        Blend
    };

    struct SceneInfo
    {
        struct View
        {
            Math::float4x4 worldToView;
            Math::float4x4 viewToClip;
            Math::float4x4 clipToWorld;

            Math::float3 cameraPosition;
            uint32_t frameIndex;

            Math::float3 front; int frontPadding;
            Math::float3 up;    int upPadding;
            Math::float3 right; int rightPadding;

            Math::float2 viewSize;
            Math::float2 viewSizeInv;

            Math::float3 focalCenter; 
            int focalCenterPadding;

            float halfWidth;
            float halfHeight;
            Math::float2 halfWidthHeightPadding;

            float minDistance = 0.1f;
            float maxDistance = 1000.0f;
            float apertureRadius = 0.003f;
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

            float rotation;
            float totalSum;
            Math::float2 size;

            float intensity;
            uint32_t descriptorIndex;
            Math::float2 envPaddding;
            
            int directionalLightCount;
            bool enableEnvironmentLight = true;
            bool padding0[3];
            Math::float2 padding1;

        } light;
        
        struct Settings
        {
            int maxLighteBounces = 8;
            int maxSamples = 1;
            RenderingMode renderingMode;
            int padding0;
           
        } settings;

        struct PostProssing
        {
            float exposure = 1.0f;
            float gamma = 2.2f;
            TonMapingType tonMappingType;
            int padding;

        } postProssing;
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
        uint32_t id;
        Math::uint firstGeometryIndex;
        Math::float4x4 transform;
    };

    struct MaterialData
    {
        Math::float4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float metallic = 0.0f;
        float roughness = 0.5f;
        float anisotropic = 0;
        float subsurface = 0;
        float specularTint = 0;
        float sheen = 0;
        float sheenTint = 0;
        float clearcoat = 0;
        float clearcoatRoughness = 0;
        float transmission = 0;
        float ior = 1.5;
        Math::float3 emissiveColor = {0.0f, 0.0f, 0.0f};

        uint32_t baseTextureIndex = c_Invalid;
        uint32_t emissiveTextureIndex = c_Invalid;
        uint32_t metallicRoughnessTextureIndex = c_Invalid;
        uint32_t normalTextureIndex = c_Invalid;

        AlfaMode alfaMode = AlfaMode::Opaque;
        float alphaCutoff = 0.01f;

        int uvSet = 0;
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
       
        uint32_t textureCount = 0;
    };

    struct FrameData
    {
        nvrhi::BindingSetHandle bindingSet;
        nvrhi::BufferHandle sceneInfoBuffer;

        nvrhi::TextureHandle accumulationOutput;
        nvrhi::TextureHandle HDRColor;
        nvrhi::TextureHandle LDRColor;
        nvrhi::TextureHandle depth;
        nvrhi::TextureHandle entitiesID;

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
    void SubmitMesh(RendererData& data, FrameData& frameData, Assets::Asset asset, Assets::Mesh& mesh, Math::float4x4 wt, uint32_t id, nvrhi::ICommandList* cl);
    void SubmitMaterial(RendererData& data, FrameData& frameData, Assets::Asset materailAsset);
    void SubmitDirectionalLight(RendererData& data, FrameData& frameData, const Assets::DirectionalLightComponent& light, Math::float4x4 wt);
    void SubmitSkyLight(RendererData& data, FrameData& frameData, Assets::SkyLightComponent& light, float rotation);
    void ReleaseTexture(RendererData& data, Assets::Texture* texture);
    void Clear(FrameData& frameData);
    nvrhi::ITexture* GetColorTarget(FrameData& frameData);
    nvrhi::ITexture* GetDepthTarget(FrameData& frameData);
    nvrhi::ITexture* GetEntitiesIDTarget(FrameData& frameData);
}