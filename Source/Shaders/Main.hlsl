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
    float4 baseColor;
    float3 emissiveColor;

    int baseTextureIndex;
    int emissiveTextureIndex;

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
    float4 color;            // rgb = surface color, a = continueBounce? (1 = yes, 0 = stop)
    float3 emissiveColor;
    float dist;              // distance to hit point
    float3 normal;
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

static const float c_RayPosNormalOffset = 0.01;
static const float c_PI = 3.14159265;
static const float c_2PI = 2.0f * c_PI;

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

uint wang_hash(inout uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}
 
float RandomFloat01(inout uint state)
{
    return float(wang_hash(state)) / 4294967296.0;
}
 
float3 RandomUnitVector(inout uint state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float a = RandomFloat01(state) * c_2PI;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}

[shader("raygeneration")]
void RayGen()
{
    uint2 rayIndex = DispatchRaysIndex().xy;
    uint rngState = uint(uint(rayIndex.x) * uint(1973) + uint(rayIndex.y) * uint(9277)) | uint(1);//+ uint(iFrame) * uint(26699)) | uint(1);
    RayDesc primaryRay = CreatePrimaryRay(float2(rayIndex), viewBuffer.clipToWorld, viewBuffer.cameraPosition, viewBuffer.viewSizeInv);

    float3 rayOrigin = primaryRay.Origin;
    float3 rayDirection = primaryRay.Direction;

    float3 resultColor = float3(0, 0, 0);
    float3 throughput = float3(1, 1, 1);

    const uint MAX_BOUNCES = 8;

    for (uint bounce = 0; bounce < MAX_BOUNCES; ++bounce)
    {
        RayDesc ray;
        ray.Origin    = rayOrigin;
        ray.Direction = rayDirection;
        ray.TMin      = 0.001f;
        ray.TMax      = 1000.0f;
    
        HitInfo payload;
        payload.color  = float4(0, 0, 0, 0);
        payload.normal = float3(0, 1, 0);
    
        TraceRay(TLAS, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    
        if (payload.color.a < 0.5f)  break; // use alpha to stop bouncing
        
        rayOrigin    = (rayOrigin + rayDirection * payload.dist) + payload.normal * c_RayPosNormalOffset;
        rayDirection = normalize(payload.normal + RandomUnitVector(rngState));                            // calculate new ray direction, in a cosine weighted hemisphere oriented at normal
        resultColor += payload.emissiveColor.rgb * throughput;                                            // add in emissive lighting
        throughput  *= payload.color.rgb;                                                                 // update the colorMultiplier   
    }

    renderTarget[rayIndex] = float4(resultColor, 1.0f);
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload : SV_RayPayload, HitAttributes attr : SV_IntersectionAttributes)
{
    uint instanceID     = InstanceID();
    uint primitiveIndex = PrimitiveIndex();
    uint geometryIndex  = GeometryIndex();

    GeometrySample gs = SampleGeometry(instanceID, primitiveIndex, geometryIndex, attr.barycentrics);
    
    float4 baseColor = gs.material.baseColor;
    if (gs.material.baseTextureIndex != c_Invalid)
    {
        Texture2D texture = bindlessTextures[NonUniformResourceIndex(gs.material.baseTextureIndex)];
        baseColor        *= texture.SampleLevel(materialSampler, gs.texcoord, 0);
    }

    float3 emissiveColor = gs.material.emissiveColor;
    if (gs.material.emissiveTextureIndex != c_Invalid)
    {
        Texture2D texture = bindlessTextures[NonUniformResourceIndex(gs.material.emissiveTextureIndex)];
        emissiveColor    *= texture.SampleLevel(materialSampler, gs.texcoord, 0).rgb;
    }

    payload.color         = float4(baseColor.rgb, 1); // a = 1 → keep bouncing
    payload.normal        = gs.geometryNormal;
    payload.emissiveColor = emissiveColor.rgb;
    payload.dist          = RayTCurrent();
}

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.color         = float4(0.1f, 0.1f, 0.1f, 1.0f);   // a = 0 → no more bounce
    payload.normal        = float3(0, 0, 0);                  // dummy
    payload.emissiveColor = float3(0.5, 0.5, 0.5);
    payload.dist          = 100000;
}
