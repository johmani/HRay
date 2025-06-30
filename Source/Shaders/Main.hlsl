#include "utils.h"

struct ViewBuffer
{
    float4x4 clipToWorld;

    float3 cameraPosition;
    int padding;

    float2 viewSize;
    float2 viewSizeInv;
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
    float4  baseColor;

    int baseTextureIndex;
    int uvSet;
    int padding0;
    int padding1;
};

VK_BINDING(0, 1) ByteAddressBuffer bindlessBuffers[] : register(t0, space1);
VK_BINDING(1, 1) Texture2D bindlessTextures[] : register(t0, space2);

RaytracingAccelerationStructure TLAS : register(t0);
StructuredBuffer<InstanceData> instanceData : register(t1);
StructuredBuffer<GeometryData> geometryData : register(t2);
StructuredBuffer<Material> materialData : register(t3);

ConstantBuffer<ViewBuffer> viewBuffer : register(b0);
SamplerState materialSampler : register(s0);
RWTexture2D<float4> renderTarget : register(u0);

typedef BuiltInTriangleIntersectionAttributes HitAttributes;

struct HitInfo
{
    float4 color : SHADED_COLOR_AND_HIT_T;
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

[shader("raygeneration")]
void RayGen()
{
    uint2 rayIndex = DispatchRaysIndex().xy;
    RayDesc ray = CreatePrimaryRay(rayIndex, viewBuffer.clipToWorld, viewBuffer.cameraPosition, viewBuffer.viewSizeInv);

    HitInfo payload;
    payload.color = float4(0, 0, 0, 0);

    TraceRay(TLAS, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    renderTarget[rayIndex] = float4(payload.color.rgb, 1.f);
}
float3 SRGBToLinear(float3 c)
{
    float3 low = c / 12.92;
    float3 high = pow((c + 0.055) / 1.055, 2.4);
    return lerp(low, high, step(0.04045, c));
}


[shader("closesthit")]
void ClosestHit(inout HitInfo payload : SV_RayPayload, HitAttributes attr : SV_IntersectionAttributes)
{
    uint instanceID = InstanceID();
    uint primitiveIndex = PrimitiveIndex();
    uint geometryIndex = GeometryIndex();

    GeometrySample gs = SampleGeometry(instanceID, primitiveIndex, geometryIndex, attr.barycentrics);
    
    float4 baseColor = gs.material.baseColor;
    if (gs.material.baseTextureIndex != c_Invalid)
    {
        Texture2D baseTexture = bindlessTextures[NonUniformResourceIndex(gs.material.baseTextureIndex)];
        baseColor *= baseTexture.SampleLevel(materialSampler, gs.texcoord, 0);
    }
    payload.color = baseColor;
    //payload.color = float4(gs.flatNormal, 1);
}

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.color = float4(0.1f, 0.1f, 0.1f, 1.0f);
}
