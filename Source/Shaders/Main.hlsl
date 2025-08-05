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

        float3 front; int frontPadding;
        float3 up;    int upPadding;
        float3 right; int rightPadding;

        float2 viewSize;
        float2 viewSizeInv;

        float3 focalCenter; int focalCenterPadding;

        float halfWidth;
        float halfHeight;
        float2 halfWidthHeightPadding;

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

        float rotation;
        float totalSum;
        float2 size;

        float intensity;
        uint descriptorIndex;
        float2 envPaddding;

        int directionalLightCount;
        bool enableEnvironmentLight;  
        float2 padding0;

    } light;

    struct Settings
    {
        int maxLighteBounces;
        int maxSamples;
        int renderingMode;
        int padding0;

    } settings;

    struct PostProssing
    {
        float exposure;
        float gamma;
        int tonMappingType;
        int padding0;

    } postProssing;
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
    uint id;
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

RWTexture2D<float4> HDRColor : register(u0);
RWTexture2D<float4> accumulationOutput : register(u1);
RWTexture2D<float4> LDRColor : register(u2);
RWTexture2D<float> depth : register(u3);
RWTexture2D<uint> entitiesID : register(u4);

typedef BuiltInTriangleIntersectionAttributes HitAttributes;

struct GeometrySample
{
    Material material;
    int entityID;
    int materialID;

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
    gs.materialID = geometry.materialIndex;

    gs.entityID = instance.id;

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

RayDesc CreatePrimaryRay(float2 uv, float4x4 clipToWorld, float3 cameraPosition)
{
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
    float3 totalSunColor = 0;
    float groundToSkyT = 1;

    if (sceneInfoBuffer.light.enableEnvironmentLight)
    {
        float3 skyColourHorizon = sceneInfoBuffer.light.skyColourHorizon.rgb;
        float3 skyColourZenith = sceneInfoBuffer.light.skyColourZenith.rgb;
        float3 groundColour = sceneInfoBuffer.light.groundColour.rgb;

        float skyT = pow(smoothstep(0.0, 0.4, rayDirection.y), 0.35);
        groundToSkyT = smoothstep(-0.01, 0.0, rayDirection.y);
        float3 skyGradient = lerp(skyColourHorizon, skyColourZenith, skyT);
        totalSunColor += lerp(groundColour, skyGradient, groundToSkyT);
    }

    for (int i = 0; i < sceneInfoBuffer.light.directionalLightCount; i++)
    {
        DirectionalLightData light = directionalLightData[i];

        float3 lightColorLinear = light.color;
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

    return totalSunColor;
}

float ComputeDepth(
    float3 cameraOrigin,
    float3 cameraForward,
    float3 hitPosition,
    float nearPlane,
    float farPlane
)
{
    float3 camToHit = hitPosition - cameraOrigin;
    float zEye = -length(camToHit) * dot(cameraForward, normalize(camToHit));
    float depthNDC = ((farPlane + nearPlane) + (2.0 * farPlane * nearPlane) / zEye) / (farPlane - nearPlane);
    return (depthNDC + 1.0f) * 0.5f;
}

float4 EvalEnvMap(
    float3 rayDirection, 
    float envMapRot, 
    float envMapTotalSum,
    float2 envMapRes,
    uint descriptorIndex
)
{
    float theta = acos(clamp(rayDirection.y, -1.0, 1.0));
    float2 uv = float2((c_PI + atan2(rayDirection.z, rayDirection.x)) * c_Inv_2PI, theta * c_Inv_PI) + float2(envMapRot, 0.0);

    float3 envColor = float3(1, 1, 1);
    if (descriptorIndex != c_Invalid)
    {
        Texture2D texture = bindlessTextures[NonUniformResourceIndex(descriptorIndex)];
        float3 texColor = texture.SampleLevel(materialSampler, uv, 0).rgb;
        envColor *= SRGBToLinear(texColor.rgb);
    }
   
    float pdf = Luminance(envColor) / envMapTotalSum;

    return float4(envColor, (pdf * envMapRes.x * envMapRes.y) / (c_2PI * c_PI * sin(theta)));
}

float PowerHeuristic(float a, float b)
{
    float t = a * a;
    return t / (b * b + t);
}

[shader("raygeneration")]
void RayGen()
{
    uint2 rayIndex = DispatchRaysIndex().xy;
    float2 ndc     = (float2(rayIndex) + 0.5) * sceneInfoBuffer.view.viewSizeInv;
    ndc            = ndc * 2.0 - 1.0;
    ndc.y          = -ndc.y; // Flip Y for DX

    uint randomNum       = rayIndex.y * (uint)sceneInfoBuffer.view.viewSize.x + rayIndex.x;
    RayDesc primaryRay   = CreatePrimaryRay(ndc, sceneInfoBuffer.view.clipToWorld, sceneInfoBuffer.view.cameraPosition);
    float3 rayOrigin     = primaryRay.Origin;
    float3 rayDirection  = primaryRay.Direction;

    float near = sceneInfoBuffer.view.minDistance;
    float far  = sceneInfoBuffer.view.maxDistance;
    primaryRay.TMin = near;
    primaryRay.TMax = far;

    float3 focusPoint;
    float3 front = sceneInfoBuffer.view.front;
    float3 up    = sceneInfoBuffer.view.up;
    float3 right = sceneInfoBuffer.view.right;
    
    float3 offset = ndc.x * sceneInfoBuffer.view.halfWidth * right + ndc.y * sceneInfoBuffer.view.halfHeight * up;
    focusPoint    = sceneInfoBuffer.view.focalCenter + offset;

    float3 finalColor = 0;
    float depthValue = 1;
    uint entityID = 4294967295u;

    for (uint i = 0; i < sceneInfoBuffer.settings.maxSamples; i++)
    {
        randomNum += (sceneInfoBuffer.view.frameIndex + i) * 895623;
       
        if (sceneInfoBuffer.view.enableDepthOfField)
        {
            float2 originOffset = RandomPointInCircle(randomNum) * sceneInfoBuffer.view.focusFalloff;
            rayOrigin           = sceneInfoBuffer.view.cameraPosition + right * originOffset.x + up * originOffset.y;
        }

        float2 targetOffset = RandomPointInCircle(randomNum) * sceneInfoBuffer.view.apertureRadius;
        rayDirection = normalize((focusPoint + right * targetOffset.x + up * targetOffset.y) - rayOrigin);

        float3 radiance = float3(0, 0, 0);
        float3 throughput  = float3(1, 1, 1);

        float pdf = 1;

        for (uint bounce = 0; bounce < sceneInfoBuffer.settings.maxLighteBounces; bounce++)
        {
            RayDesc ray;
            ray.Origin    = rayOrigin;
            ray.Direction = rayDirection;
            ray.TMin      = near;
            ray.TMax      = far;
        
            HitInfo payload;
            RAY_FLAG flags = RAY_FLAG_NONE;//RAY_FLAG_CULL_BACK_FACING_TRIANGLES; // RAY_FLAG_NONE
            TraceRay(TLAS, flags, 0xFF, 0, 0, 0, ray, payload);
            float3 hitPoint = rayOrigin + rayDirection * payload.distance;
        
            if (sceneInfoBuffer.view.enableVisualFocusDistance && bounce == 0 && length(hitPoint - focusPoint) <= 0.2)
                radiance = lerp(radiance, float3(0, 1, 0), 0.1);

            if (payload.HasHit())
            {
                if (bounce == 0)
                {
                    depthValue = ComputeDepth(rayOrigin, sceneInfoBuffer.view.front, hitPoint, near, far);
                    entityID = payload.entityID;
                }

                if (sceneInfoBuffer.settings.renderingMode == c_RenderingMode_Normals)
                {
                    radiance = payload.normal;
                    break;
                }
                else if (sceneInfoBuffer.settings.renderingMode == c_RenderingMode_Tangent)
                {
                    radiance = payload.tangent;
                    break;
                }
                else if (sceneInfoBuffer.settings.renderingMode == c_RenderingMode_Bitangent)
                {
                    radiance = payload.bitangent;
                    break;
                }

                radiance += payload.emissive * throughput;
                throughput *= payload.baseColor.rgb;

                float3 diffuse = normalize(payload.normal + RandomDirection(randomNum));
                float3 specular = reflect(rayDirection, payload.normal);

                rayOrigin = hitPoint + payload.normal * c_RayPosNormalOffset;
                rayDirection = normalize(lerp(specular, diffuse, payload.roughness));

                // Russian roulette
                if (bounce > 2)
                {
                    float q = min(max(throughput.x, max(throughput.y, throughput.z)) + 0.001, 0.95);
                    if (RandomFloat(randomNum) > q) break;
                    throughput /= q;
                }
            }
            else
            {
                if (sceneInfoBuffer.light.descriptorIndex == c_Invalid)
                {
                    radiance += EvaluateEnvironmentLight(rayOrigin, rayDirection) * throughput;
                }
                else
                {
                    float4 envMapColPdf = EvalEnvMap(
                        rayDirection,
                        sceneInfoBuffer.light.rotation,
                        sceneInfoBuffer.light.totalSum,
                        sceneInfoBuffer.light.size,
                        sceneInfoBuffer.light.descriptorIndex
                    );

                    float misWeight = 1.0;

                    if (bounce > 0)
                        misWeight = PowerHeuristic(pdf, envMapColPdf.w);

                    if (misWeight > 0)
                        radiance += misWeight * envMapColPdf.rgb * throughput * sceneInfoBuffer.light.intensity;

                }

                break;
            }
        }
    
        finalColor += radiance;
    }

    finalColor = finalColor / sceneInfoBuffer.settings.maxSamples;
    HDRColor[rayIndex] = float4(finalColor, 1);
    depth[rayIndex] = depthValue;
    entitiesID[rayIndex] = entityID;

    // Accumulation
    {
        float3 prev = accumulationOutput[rayIndex].rgb;
        float3 curr = HDRColor[rayIndex].rgb;
        uint frameIndex = sceneInfoBuffer.view.frameIndex;
        float3 accumulated = (prev * frameIndex + curr) / (frameIndex + 1);
        HDRColor[rayIndex] = float4(accumulated, 1);
    }
    
    // Tone Mapping
    {
        float3 color = HDRColor[rayIndex].rgb;
    
        switch (sceneInfoBuffer.postProssing.tonMappingType)
        {
        case c_TonMapingType_None:                                                            break;
        case c_TonMapingType_WhatEver:   color = Tonemap(color, 1.5);                         break;
        case c_TonMapingType_ACES:       color = ACES(color);                                 break;
        case c_TonMapingType_ACESFitted: color = ACESFitted(color);                           break;
        case c_TonMapingType_Filmic:     color = Filmic(color, sceneInfoBuffer.postProssing.exposure);    break;
        case c_TonMapingType_Reinhard:   color = Reinhard(color, sceneInfoBuffer.postProssing.exposure);  break;
        }
        
        color = color = pow(color, 1 / sceneInfoBuffer.postProssing.gamma);
        LDRColor[rayIndex] = float4(color, 1);
    }
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload : SV_RayPayload, HitAttributes attr : SV_IntersectionAttributes)
{
    uint instanceID     = InstanceID();
    uint primitiveIndex = PrimitiveIndex();
    uint geometryIndex  = GeometryIndex();
    float3 rayDirection = WorldRayDirection();

    GeometrySample gs = SampleGeometry(instanceID, primitiveIndex, geometryIndex, attr.barycentrics);

    float2 uv = mul(float3(gs.texcoord, 1.0), gs.material.uvMat).xy;
    
    float4 baseColor = gs.material.baseColor;
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
    float roughness = max(gs.material.roughness, 0.001);
    if (gs.material.metallicRoughnessTextureIndex != c_Invalid)
    {
        Texture2D texture = bindlessTextures[NonUniformResourceIndex(gs.material.metallicRoughnessTextureIndex)];
        float3 texColor   = texture.SampleLevel(materialSampler, uv, 0).rgb;
        metallic          = texColor.b;
        roughness         = max(texColor.g * texColor.g, 0.001);
    }

    {
        float3 n = gs.geometryNormal;
        float3 t = gs.tangent.xyz;
        float3 b = normalize(cross(n, t) * gs.tangent.w);

        payload.normal = n;
        payload.tangent = t;
        payload.bitangent = b;

        if (gs.material.normalTextureIndex != c_Invalid)
        {
            Texture2D texture = bindlessTextures[NonUniformResourceIndex(gs.material.normalTextureIndex)];
            float3 texNormal = texture.SampleLevel(materialSampler, uv, 0).rgb;
            texNormal = normalize(texNormal * 2.0f - 1.0f);
            float3x3 tbn = float3x3(t, b, n);
            payload.normal = normalize(mul(texNormal, tbn));
        }

        payload.ffnormal = dot(n, -rayDirection) > 0.0 ? payload.normal : -payload.normal;
    }

    payload.baseColor          = baseColor.rgb;
    payload.metallic           = metallic;
    payload.roughness          = roughness;
    payload.emissive           = emissiveColor;
    payload.distance           = RayTCurrent();
    payload.entityID           = gs.entityID;
}

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.distance = 1000;
}
