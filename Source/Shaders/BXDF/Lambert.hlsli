#ifndef LAMBERT_H
#define LAMBERT_H

float3 EvaluateBRDF(HitInfo hitInfo, float3 V, float3 N, float3 L, inout float pdf)
{
    pdf = dot(N, L) * (1.0 / c_PI);

    return (1.0 / c_PI) * hitInfo.baseColor * dot(N, L);
}

float3 SampleBRDF(HitInfo hitInfo, float3 V, float3 N, inout float3 L, inout float pdf, inout uint randomNum)
{
    float3 T = hitInfo.tangent;
    float3 B = hitInfo.bitangent;

    float r1 = RandomFloat(randomNum);
    float r2 = RandomFloat(randomNum);

    L = CosineSampleHemisphere(r1, r2);
    L = T * L.x + B * L.y + N * L.z;

    return EvaluateBRDF(hitInfo, V, N, L, pdf);
}

#endif // LAMBERT_H