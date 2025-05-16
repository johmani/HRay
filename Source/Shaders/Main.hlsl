struct ViewBuffer
{
    float4x4 clipToWorld;

    float3 cameraPosition;
    int padding;

    float2 viewSize;
    float2 viewSizeInv;
};

RWTexture2D<float4> renderTarget      : register(u0);
RaytracingAccelerationStructure TLAS  : register(t0);
ConstantBuffer<ViewBuffer> viewBuffer : register(b0);

typedef BuiltInTriangleIntersectionAttributes HitAttributes;

struct HitInfo
{
    float4 color;
};

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

[shader("closesthit")]
void ClosestHit(inout HitInfo payload : SV_RayPayload, HitAttributes attr : SV_IntersectionAttributes)
{
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    payload.color = float4(barycentrics, 1);
}

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.color = float4(0.1f, 0.1f, 0.1f, 1.0f);
}
