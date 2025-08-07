#ifndef BRDF_H
#define BRDF_H

float3 CosineSampleHemisphere(float r1, float r2)
{
    float3 dir;
    float r = sqrt(r1);
    float phi = c_2PI * r2;
    dir.x = r * cos(phi);
    dir.y = r * sin(phi);
    dir.z = sqrt(max(0.0, 1.0 - dir.x * dir.x - dir.y * dir.y));
    return dir;
}

#ifdef LAMBERT

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

#endif

#ifdef SIMPLE

float3 SampleBRDF(HitInfo hitInfo, float3 V, float3 N, inout float3 L, inout float pdf, inout uint randomNum)
{
    float3 diffuse = normalize(hitInfo.normal + RandomDirection(randomNum));
    float3 specular = reflect(-V, hitInfo.normal);
    L = normalize(lerp(specular, diffuse, hitInfo.roughness));
    pdf = 1;

    return hitInfo.baseColor.rgb;
}

#endif


#endif // BRDF_H