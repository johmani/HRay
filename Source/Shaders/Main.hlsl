#include "Base.hlsli"

struct SceneInfo
{
    struct View
    {
        float4x4 worldToView;
        float4x4 viewToClip;
        float4x4 clipToWorld;

        float3 cameraPosition;
        int frameIndex;

        float2 viewSize;
        float2 viewSizeInv;

        float minDistance;
        float maxDistance;
        float apertureRadius;
        float focusFalloff;

        float focusDistance;
        float fov;
        float2 padding0;

        bool enableVisualFocusDistance;
        bool enableDepthOfField;
        float2 padding1;

    } view;

    struct Light
    {
        float4 groundColour;
        float4 skyColourHorizon;
        float4 skyColourZenith;

        int directionalLightCount;
        bool enableEnvironmentLight;  
        float2 padding0;

    } light;

    struct Settings
    {
        int maxLighteBounces;
        int maxSamples;
        float gamma;
        int padding0;

    } settings;
};

struct GeometryData
{
    uint indexBufferIndex;
    uint vertexBufferIndex;

    uint indexCount;
    uint vertexCount;

    uint indexOffset;
    uint positionOffset;
    uint normalOffset;
    uint tangentOffset;
    uint texCoord0Offset;
    uint texCoord1Offset;

    uint materialIndex;
};

struct InstanceData
{
    uint firstGeometryIndex;
    float4x4 transform;
};

struct Material
{
    float4 baseColor;
    float metallic;
    float roughness;
    int uvSet;
    int padding0;

    float3 emissiveColor;

    uint baseTextureIndex;
    uint emissiveTextureIndex;
    uint metallicRoughnessTextureIndex;
    uint normalTextureIndex;

    float3x3 uvMat;
};

struct DirectionalLightData
{
    float3 direction;
    float3 color;
    float intensity;
    float angularRadius;
    float haloSize;
    float haloFalloff;
};

VK_BINDING(0, 1) ByteAddressBuffer bindlessBuffers[] : register(t0, space1);
VK_BINDING(1, 1) Texture2D bindlessTextures[] : register(t0, space2);

RaytracingAccelerationStructure TLAS : register(t0);
StructuredBuffer<InstanceData> instanceData : register(t1);
StructuredBuffer<GeometryData> geometryData : register(t2);
StructuredBuffer<Material> materialData : register(t3);
StructuredBuffer<DirectionalLightData> directionalLightData : register(t4);

ConstantBuffer<SceneInfo> sceneInfoBuffer : register(b0);

SamplerState materialSampler : register(s0);
RWTexture2D<float4> renderTarget : register(u0);
RWTexture2D<float4> prevRenderTarget : register(u1);

typedef BuiltInTriangleIntersectionAttributes HitAttributes;

struct HitInfo
{
    float4 color;
    float3 emissiveColor;
    float3 normal;
    float distance;
    float metallic;
    float roughness;
};

struct GeometrySample
{
    Material material;

    float3 objectSpacePosition;
    float4 tangent;
    float3 flatNormal;
    float3 geometryNormal;
    float2 texcoord;
};

GeometrySample SampleGeometry(
    uint instanceIndex,
    uint triangleIndex,
    uint geometryIndex,
    float2 rayBarycentrics
)
{
    GeometrySample gs;

    InstanceData instance = instanceData[instanceIndex];
    GeometryData geometry = geometryData[instance.firstGeometryIndex + geometryIndex];
    gs.material = materialData[geometry.materialIndex];

    ByteAddressBuffer indexBuffer = bindlessBuffers[NonUniformResourceIndex(geometry.indexBufferIndex)];
    ByteAddressBuffer vertexBuffer = bindlessBuffers[NonUniformResourceIndex(geometry.vertexBufferIndex)];

    float3 barycentrics = float3(1 - rayBarycentrics.x - rayBarycentrics.y, rayBarycentrics.x, rayBarycentrics.y);

    uint3 indices = indexBuffer.Load3(geometry.indexOffset + triangleIndex * c_SizeOfTriangleIndices);

    float3 vertexPositions[3];
    {
        vertexPositions[0] = asfloat(vertexBuffer.Load3(geometry.positionOffset + indices[0] * c_SizeOfPosition));
        vertexPositions[1] = asfloat(vertexBuffer.Load3(geometry.positionOffset + indices[1] * c_SizeOfPosition));
        vertexPositions[2] = asfloat(vertexBuffer.Load3(geometry.positionOffset + indices[2] * c_SizeOfPosition));
        gs.objectSpacePosition = Interpolate(vertexPositions, barycentrics);
    }

    if (geometry.normalOffset != c_Invalid)
    {
        float3 normals[3];
        normals[0] = Unpack_RGB8_SNORM(vertexBuffer.Load(geometry.normalOffset + indices[0] * c_SizeOfNormal));
        normals[1] = Unpack_RGB8_SNORM(vertexBuffer.Load(geometry.normalOffset + indices[1] * c_SizeOfNormal));
        normals[2] = Unpack_RGB8_SNORM(vertexBuffer.Load(geometry.normalOffset + indices[2] * c_SizeOfNormal));
        gs.geometryNormal = Interpolate(normals, barycentrics);
        gs.geometryNormal = mul(instance.transform, float4(gs.geometryNormal, 0.0)).xyz;
        gs.geometryNormal = normalize(gs.geometryNormal);
    }

    if (geometry.tangentOffset != c_Invalid)
    {
        float4 tangents[3];
        tangents[0] = Unpack_RGBA8_SNORM(vertexBuffer.Load(geometry.tangentOffset + indices[0] * c_SizeOfNormal));
        tangents[1] = Unpack_RGBA8_SNORM(vertexBuffer.Load(geometry.tangentOffset + indices[1] * c_SizeOfNormal));
        tangents[2] = Unpack_RGBA8_SNORM(vertexBuffer.Load(geometry.tangentOffset + indices[2] * c_SizeOfNormal));
        gs.tangent.xyz = Interpolate(tangents, barycentrics).xyz;
        gs.tangent.xyz = mul(instance.transform, float4(gs.tangent.xyz, 0.0)).xyz;
        gs.tangent.xyz = normalize(gs.tangent.xyz);
        gs.tangent.w = tangents[0].w;
    }

    if (gs.material.uvSet == 0 && geometry.texCoord0Offset != c_Invalid)
    {
        float2 vertexTexcoords[3];
        vertexTexcoords[0] = asfloat(vertexBuffer.Load2(geometry.texCoord0Offset + indices[0] * c_SizeOfTexcoord));
        vertexTexcoords[1] = asfloat(vertexBuffer.Load2(geometry.texCoord0Offset + indices[1] * c_SizeOfTexcoord));
        vertexTexcoords[2] = asfloat(vertexBuffer.Load2(geometry.texCoord0Offset + indices[2] * c_SizeOfTexcoord));
        gs.texcoord = Interpolate(vertexTexcoords, barycentrics);
    }

    if (gs.material.uvSet == 1 && geometry.texCoord1Offset != c_Invalid)
    {
        float2 vertexTexcoords[3];
        vertexTexcoords[0] = asfloat(vertexBuffer.Load2(geometry.texCoord1Offset + indices[0] * c_SizeOfTexcoord));
        vertexTexcoords[1] = asfloat(vertexBuffer.Load2(geometry.texCoord1Offset + indices[1] * c_SizeOfTexcoord));
        vertexTexcoords[2] = asfloat(vertexBuffer.Load2(geometry.texCoord1Offset + indices[2] * c_SizeOfTexcoord));
        gs.texcoord = Interpolate(vertexTexcoords, barycentrics);
    }

    float3 objectSpaceFlatNormal = normalize(cross(vertexPositions[1] - vertexPositions[0], vertexPositions[2] - vertexPositions[0]));
    gs.flatNormal = normalize(mul(instance.transform, float4(objectSpaceFlatNormal, 0.0)).xyz);

    return gs;
}

RayDesc CreatePrimaryRay(float2 id, float4x4 clipToWorld, float3 cameraPosition, float2 viewSizeInv)
{
    float2 uv = (float2(id.xy) + 0.5) * viewSizeInv;
    uv.y = 1 - uv.y;
    uv = uv * 2.0 - 1.0;

    float4 clipPos = float4(uv, 1.0, 1.0);

    float4 worldPos = mul(clipToWorld, clipPos);
    worldPos.xyz /= worldPos.w;

    RayDesc ray;
    ray.Origin = cameraPosition;
    ray.Direction = normalize(worldPos.xyz - cameraPosition);
    ray.TMin = 0;
    ray.TMax = 1000;

    return ray;
}

float3 EvaluateEnvironmentLight(float3 rayOrigin, float3 rayDirection)
{
    if (!sceneInfoBuffer.light.enableEnvironmentLight)
        return 0;

    float3 skyColourHorizon = SRGBToLinear(sceneInfoBuffer.light.skyColourHorizon.rgb);
    float3 skyColourZenith = SRGBToLinear(sceneInfoBuffer.light.skyColourZenith.rgb);
    float3 groundColour = SRGBToLinear(sceneInfoBuffer.light.groundColour.rgb);

    float skyT = pow(smoothstep(0.0, 0.4, rayDirection.y), 0.35);
    float groundToSkyT = smoothstep(-0.01, 0.0, rayDirection.y);
    float3 skyGradient = lerp(skyColourHorizon, skyColourZenith, skyT);
    float3 skyColor = lerp(groundColour, skyGradient, groundToSkyT);

    float3 totalSunColor = 0;

    for (int i = 0; i < sceneInfoBuffer.light.directionalLightCount; i++)
    {
        DirectionalLightData light = directionalLightData[i];

        float3 lightColorLinear = SRGBToLinear(light.color);
        float cosTheta = dot(normalize(rayDirection), -light.direction);
        float softness = 0.05;
        float sunDisk = smoothstep(cos(light.angularRadius + softness), cos(light.angularRadius), cosTheta);
        float haloStart = light.angularRadius;
        float haloEnd = light.angularRadius + light.haloSize;
        float halo = smoothstep(cos(haloEnd), cos(haloStart), cosTheta);
        //halo = pow(halo, light.haloFalloff);
        float sunIntensity = sunDisk * light.intensity;
        float haloIntensity = halo * light.intensity;

        float3 sunContribution = (sunIntensity + haloIntensity) * lightColorLinear * (groundToSkyT >= 1);

        totalSunColor += sunContribution;
    }

    return skyColor + totalSunColor;
}

[shader("raygeneration")]
void RayGen()
{
    uint2 rayIndex       = DispatchRaysIndex().xy;
    uint rngState        = rayIndex.y * (uint)sceneInfoBuffer.view.viewSize.x + rayIndex.x;
    RayDesc primaryRay   = CreatePrimaryRay(float2(rayIndex), sceneInfoBuffer.view.clipToWorld, sceneInfoBuffer.view.cameraPosition, sceneInfoBuffer.view.viewSizeInv);
    float3 rayOrigin     = primaryRay.Origin;
    float3 rayDirection  = primaryRay.Direction;

    float2 uv  = (float2(rayIndex) + 0.5) / sceneInfoBuffer.view.viewSize;
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y      = -ndc.y; // Flip Y for DX

    float near = sceneInfoBuffer.view.minDistance;
    float far  = sceneInfoBuffer.view.maxDistance;
    primaryRay.TMin = near;
    primaryRay.TMax = far;

    float3 right, up;
    float3 focusPoint;
    if(sceneInfoBuffer.view.enableDepthOfField)
    {
        GetCameraRightUp(sceneInfoBuffer.view.clipToWorld, right, up);
        float3 focalCenter = (sceneInfoBuffer.view.cameraPosition + normalize(cross(up, right)) * sceneInfoBuffer.view.focusDistance);
        float aspect = sceneInfoBuffer.view.viewSize.x / sceneInfoBuffer.view.viewSize.y;
        float halfHeight = tan(sceneInfoBuffer.view.fov * 0.5) * sceneInfoBuffer.view.focusDistance;
        float halfWidth = halfHeight * aspect;
        float3 offset = ndc.x * halfWidth * right + ndc.y * halfHeight * up;
        focusPoint = focalCenter + offset;
    }

    float3 finalColor = 0;

    for (uint i = 0; i < sceneInfoBuffer.settings.maxSamples; i++)
    {
        rngState += (sceneInfoBuffer.view.frameIndex + i) * 895623;
        float2 originOffset = RandomPointInCircle(rngState) * sceneInfoBuffer.view.focusFalloff / sceneInfoBuffer.view.viewSize.x;
        float2 targetOffset   = RandomPointInCircle(rngState) * sceneInfoBuffer.view.apertureRadius / sceneInfoBuffer.view.viewSize.x;
        
        if (sceneInfoBuffer.view.enableDepthOfField)
        {
            rayOrigin    = sceneInfoBuffer.view.cameraPosition + right * originOffset.x + up * originOffset.y;
            rayDirection = normalize((focusPoint + right * targetOffset.x + up * targetOffset.y) - rayOrigin);
        }

        float3 resultColor = float3(0, 0, 0);
        float3 throughput  = float3(1, 1, 1);
    
        for (uint bounce = 0; bounce < sceneInfoBuffer.settings.maxLighteBounces; bounce++)
        {
            RayDesc ray;
            ray.Origin    = rayOrigin;
            ray.Direction = rayDirection;
            ray.TMin      = near;
            ray.TMax      = far;
        
            HitInfo payload;
            //RAY_FLAG flags = RAY_FLAG_NONE;
            RAY_FLAG flags = RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
            TraceRay(TLAS, flags, 0xFF, 0, 0, 0, ray, payload);
            float3 hitPoint = rayOrigin + rayDirection * payload.distance;
        
            if (sceneInfoBuffer.view.enableVisualFocusDistance && bounce == 0 && length(hitPoint - focusPoint) <= 0.2)
                resultColor = lerp(resultColor, float3(0, 1, 0), 0.1);

            if (payload.distance < 1000)
            {
                rayOrigin = hitPoint + payload.normal * c_RayPosNormalOffset;
                float3 diffuse = normalize(payload.normal + RandomDirection(rngState));
                float3 specular = reflect(rayDirection, payload.normal);

                rayDirection = normalize(lerp(specular, diffuse, payload.roughness));
                resultColor += payload.emissiveColor * throughput;
                throughput *= payload.color.rgb;
            }
            else
            {
               resultColor += EvaluateEnvironmentLight(rayOrigin, rayDirection) * throughput;
               break;
            }
        }
    
        finalColor += resultColor;
    }

    finalColor = finalColor / sceneInfoBuffer.settings.maxSamples;
    finalColor = LinearToSRGB(finalColor);
    renderTarget[rayIndex] = float4(finalColor, 1);

    // Accumulation
    {
        float3 c0 = SRGBToLinear(prevRenderTarget[rayIndex].rgb);
        float3 c1 = SRGBToLinear(renderTarget[rayIndex].rgb);

        float t = 1.0 / (sceneInfoBuffer.view.frameIndex + 1);
        renderTarget[rayIndex] = float4(LinearToSRGB(lerp(c0, c1, t)), 1);
    }
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload : SV_RayPayload, HitAttributes attr : SV_IntersectionAttributes)
{
    uint instanceID     = InstanceID();
    uint primitiveIndex = PrimitiveIndex();
    uint geometryIndex  = GeometryIndex();

    GeometrySample gs = SampleGeometry(instanceID, primitiveIndex, geometryIndex, attr.barycentrics);

    float2 uv = mul(float3(gs.texcoord, 1.0), gs.material.uvMat).xy;
    
    float4 baseColor = float4(SRGBToLinear(gs.material.baseColor.rgb), gs.material.baseColor.a);
    if (gs.material.baseTextureIndex != c_Invalid)
    {
        Texture2D texture  = bindlessTextures[NonUniformResourceIndex(gs.material.baseTextureIndex)];
        float4 texColor    = texture.SampleLevel(materialSampler, uv, 0);
        baseColor         *= float4(SRGBToLinear(texColor.rgb), texColor.a);
    }

    float3 emissiveColor = gs.material.emissiveColor;
    if (gs.material.emissiveTextureIndex != c_Invalid)
    {
        Texture2D texture  = bindlessTextures[NonUniformResourceIndex(gs.material.emissiveTextureIndex)];
        float3 texColor    = texture.SampleLevel(materialSampler, uv, 0).rgb;
        emissiveColor     *= SRGBToLinear(texColor);
    }

    float metallic  = gs.material.metallic;
    float roughness = gs.material.roughness;
    if (gs.material.metallicRoughnessTextureIndex != c_Invalid)
    {
        Texture2D texture = bindlessTextures[NonUniformResourceIndex(gs.material.metallicRoughnessTextureIndex)];
        float3 texColor   = texture.SampleLevel(materialSampler, uv, 0).rgb;
        metallic          = texColor.r;
        roughness         = texColor.g;
    }

    if (gs.material.normalTextureIndex != c_Invalid)
    {
        Texture2D texture = bindlessTextures[NonUniformResourceIndex(gs.material.normalTextureIndex)];
        float3 texColor   = texture.SampleLevel(materialSampler, uv, 0).rgb;
        texColor          = normalize(texColor * 2.0f - 1.0f);
        float3 t          = normalize(gs.tangent.xyz);
        float3 n          = normalize(gs.geometryNormal);
        float3 b          = normalize(cross(n, t) * gs.tangent.w);
        float3x3 mat      = float3x3(t, b, n);
        payload.normal    = normalize(mul(texColor, mat));
    }
    else
    {
        payload.normal = normalize(gs.geometryNormal);
    }

    payload.color         = baseColor;
    payload.metallic      = metallic;
    payload.roughness     = roughness;
    payload.emissiveColor = emissiveColor;
    payload.distance      = RayTCurrent();
}

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.distance = 1000;
}
