#ifndef DISNEY_H
#define DISNEY_H

float3 SampleGGXVNDF(float3 V, float ax, float ay, float r1, float r2)
{
    float3 Vh = normalize(float3(ax * V.x, ay * V.y, V.z));

    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0 ? float3(-Vh.y, Vh.x, 0) * rsqrt(lensq) : float3(1, 0, 0);
    float3 T2 = cross(Vh, T1);

    float r = sqrt(r1);
    float phi = 2.0 * c_PI * r2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

    return normalize(float3(ax * Nh.x, ay * Nh.y, max(0.0, Nh.z)));
}

float DielectricFresnel(float cosThetaI, float eta)
{
    float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);

    // Total internal reflection
    if (sinThetaTSq > 1.0)
        return 1.0;

    float cosThetaT = sqrt(max(1.0 - sinThetaTSq, 0.0));

    float rs = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);
    float rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

    return 0.5f * (rs * rs + rp * rp);
}

float GTR1(float NDotH, float a)
{
    if (a >= 1.0)
        return c_Inv_PI;
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
    return (a2 - 1.0) / (c_PI * log(a2) * t);
}

float SmithG(float NDotV, float alphaG)
{
    float a = alphaG * alphaG;
    float b = NDotV * NDotV;
    return (2.0 * NDotV) / (NDotV + sqrt(a + b - a * b));
}

float3 SampleGTR1(float rgh, float r1, float r2)
{
    float a = max(0.001, rgh);
    float a2 = a * a;

    float phi = r1 * c_2PI;

    float cosTheta = sqrt((1.0 - pow(a2, 1.0 - r2)) / (1.0 - a2));
    float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
    float sinPhi = sin(phi);
    float cosPhi = cos(phi);

    return float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

float SchlickWeight(float u)
{
    float m = clamp(1.0 - u, 0.0, 1.0);
    float m2 = m * m;
    return m2 * m2 * m; // pow(m,5)
}

float GTR2Aniso(float NDotH, float HDotX, float HDotY, float ax, float ay)
{
    float a = HDotX / ax;
    float b = HDotY / ay;
    float c = a * a + b * b + NDotH * NDotH;
    return 1.0 / (c_PI * ax * ay * c * c);
}

float SmithGAniso(float NDotV, float VDotX, float VDotY, float ax, float ay)
{
    float a = VDotX * ax;
    float b = VDotY * ay;
    float c = NDotV;
    return (2.0 * NDotV) / (NDotV + sqrt(a * a + b * b + c * c));
}

void TintColors(HitInfo hitInfo, out float F0, out float3 Csheen, out float3 Cspec0)
{
    float lum = Luminance(hitInfo.baseColor);
    float3 ctint = lum > 0.0 ? hitInfo.baseColor / lum : 1.0;

    F0 = (1.0 - hitInfo.eta) / (1.0 + hitInfo.eta);
    F0 *= F0;

    Cspec0 = F0 * lerp(1.0, ctint, hitInfo.specularTint);
    Csheen = lerp(1.0, ctint, hitInfo.sheenTint);
}

float3 EvalDisneyDiffuse(HitInfo hitInfo, float3 Csheen, float3 V, float3 L, float3 H, out float pdf)
{
    pdf = 0.0;
    if (L.z <= 0.0)
        return 0.0;

    float LDotH = dot(L, H);

    float Rr = 2.0 * hitInfo.roughness * LDotH * LDotH;

    // Diffuse
    float FL = SchlickWeight(L.z);
    float FV = SchlickWeight(V.z);
    float Fretro = Rr * (FL + FV + FL * FV * (Rr - 1.0));
    float Fd = (1.0 - 0.5 * FL) * (1.0 - 0.5 * FV);

    // Fake subsurface
    float Fss90 = 0.5 * Rr;
    float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1.0 / (L.z + V.z) - 0.5) + 0.5);

    // Sheen
    float FH = SchlickWeight(LDotH);
    float3 Fsheen = FH * hitInfo.sheen * Csheen;

    pdf = L.z * c_Inv_PI;
    return c_Inv_PI * hitInfo.baseColor * lerp(Fd + Fretro, ss, hitInfo.subsurface) + Fsheen;
}

float3 EvalMicrofacetReflection(HitInfo hitInfo, float3 V, float3 L, float3 H, float3 F, out float pdf)
{
    pdf = 0.0;
    if (L.z <= 0.0)
        return 0.0;

    float D = GTR2Aniso(H.z, H.x, H.y, hitInfo.ax, hitInfo.ay);
    float G1 = SmithGAniso(abs(V.z), V.x, V.y, hitInfo.ax, hitInfo.ay);
    float G2 = G1 * SmithGAniso(abs(L.z), L.x, L.y, hitInfo.ax, hitInfo.ay);

    pdf = G1 * D / (4.0 * V.z);
    return F * D * G2 / (4.0 * L.z * V.z);
}

float3 EvalMicrofacetRefraction(HitInfo hitInfo, float eta, float3 V, float3 L, float3 H, float3 F, out float pdf)
{
    pdf = 0.0;
    if (L.z >= 0.0)
        return 0.0;

    float LDotH = dot(L, H);
    float VDotH = dot(V, H);

    float D = GTR2Aniso(H.z, H.x, H.y, hitInfo.ax, hitInfo.ay);
    float G1 = SmithGAniso(abs(V.z), V.x, V.y, hitInfo.ax, hitInfo.ay);
    float G2 = G1 * SmithGAniso(abs(L.z), L.x, L.y, hitInfo.ax, hitInfo.ay);
    float denom = LDotH + VDotH * eta;
    denom *= denom;
    float eta2 = eta * eta;
    float jacobian = abs(LDotH) / denom;

    pdf = G1 * max(0.0, VDotH) * D * jacobian / V.z;
    return pow(hitInfo.baseColor, 0.5) * (1.0 - F) * D * G2 * abs(VDotH) * jacobian * eta2 / abs(L.z * V.z);
}

float3 EvalClearcoat(HitInfo hitInfo, float3 V, float3 L, float3 H, out float pdf)
{
    pdf = 0.0;
    if (L.z <= 0.0)
        return 0.0;

    float VDotH = dot(V, H);

    float F = lerp(0.04, 1.0, SchlickWeight(VDotH));
    float D = GTR1(H.z, hitInfo.clearcoatRoughness);
    float G = SmithG(L.z, 0.25) * SmithG(V.z, 0.25);
    float jacobian = 1.0 / (4.0 * VDotH);

    pdf = D * H.z * jacobian;
    return float3(F, F, F) * D * G;
}

float3 EvaluateBRDF(HitInfo hitInfo, float3 V, float3 N, float3 L, out float pdf)
{
    pdf = 0.0;
    float3 f = 0.0;

    float3 T, B;
    Onb(N, T, B);

    // Transform to shading space to simplify operations (NDotL = L.z; NDotV = V.z; NDotH = H.z)
    V = ToLocal(T, B, N, V);
    L = ToLocal(T, B, N, L);

    float3 H;
    if (L.z > 0.0)
        H = normalize(L + V);
    else
        H = normalize(L + V * hitInfo.eta);

    if (H.z < 0.0)
        H = -H;

    // Tint colors
    float3 Csheen;
    float3 Cspec0;
    float F0;
    TintColors(hitInfo, F0, Csheen, Cspec0);

    // Model weights
    float dielectricWt = (1.0 - hitInfo.metallic) * (1.0 - hitInfo.transmission);
    float metalWt = hitInfo.metallic;
    float glassWt = (1.0 - hitInfo.metallic) * hitInfo.transmission;

    // Lobe probabilities
    float schlickWt = SchlickWeight(V.z);

    float diffPr = dielectricWt * Luminance(hitInfo.baseColor);
    float dielectricPr = dielectricWt * Luminance(lerp(Cspec0, float3(1.0, 1.0, 1.0), schlickWt));
    float metalPr = metalWt * Luminance(lerp(hitInfo.baseColor, float3(1.0, 1.0, 1.0), schlickWt));
    float glassPr = glassWt;
    float clearCtPr = 0.25 * hitInfo.clearcoat;

    // Normalize probabilities
    float invTotalWt = 1.0 / (diffPr + dielectricPr + metalPr + glassPr + clearCtPr);
    diffPr *= invTotalWt;
    dielectricPr *= invTotalWt;
    metalPr *= invTotalWt;
    glassPr *= invTotalWt;
    clearCtPr *= invTotalWt;

    bool reflect = L.z * V.z > 0;

    float tmpPdf = 0.0;
    float VDotH = abs(dot(V, H));

    // Diffuse
    if (diffPr > 0.0 && reflect)
    {
        f += EvalDisneyDiffuse(hitInfo, Csheen, V, L, H, tmpPdf) * dielectricWt;
        pdf += tmpPdf * diffPr;
    }

    // Dielectric Reflection
    if (dielectricPr > 0.0 && reflect)
    {
        // Normalize for interpolating based on Cspec0
        float F = (DielectricFresnel(VDotH, 1.0 / hitInfo.ior) - F0) / (1.0 - F0);

        f += EvalMicrofacetReflection(hitInfo, V, L, H, lerp(Cspec0, float3(1.0, 1.0, 1.0), F), tmpPdf) * dielectricWt;
        pdf += tmpPdf * dielectricPr;
    }

    // Metallic Reflection
    if (metalPr > 0.0 && reflect)
    {
        // Tinted to base color
        float3 F = lerp(hitInfo.baseColor, float3(1.0, 1.0, 1.0), SchlickWeight(VDotH));

        f += EvalMicrofacetReflection(hitInfo, V, L, H, F, tmpPdf) * metalWt;
        pdf += tmpPdf * metalPr;
    }

    // Glass/Specular BSDF
    if (glassPr > 0.0)
    {
        // Dielectric fresnel (achromatic)
        float F = DielectricFresnel(VDotH, hitInfo.eta);

        if (reflect)
        {
            f += EvalMicrofacetReflection(hitInfo, V, L, H, float3(F, F, F), tmpPdf) * glassWt;
            pdf += tmpPdf * glassPr * F;
        }
        else
        {
            f += EvalMicrofacetRefraction(hitInfo, hitInfo.eta, V, L, H, float3(F, F, F), tmpPdf) * glassWt;
            pdf += tmpPdf * glassPr * (1.0 - F);
        }
    }

    // Clearcoat
    if (clearCtPr > 0.0 && reflect)
    {
        f += EvalClearcoat(hitInfo, V, L, H, tmpPdf) * 0.25 * hitInfo.clearcoat;
        pdf += tmpPdf * clearCtPr;
    }

    return f * abs(L.z);
}

float3 SampleBRDF(HitInfo hitInfo, float3 V, float3 N, out float3 L, out float pdf, inout uint random)
{
    pdf = 0.0;

    float r1 = RandomFloat(random);
    float r2 = RandomFloat(random);

    float3 T, B;
    Onb(N, T, B);

    // Transform to shading space to simplify operations (NDotL = L.z; NDotV = V.z; NDotH = H.z)
    V = ToLocal(T, B, N, V);

    // Tint colors
    float3 Csheen, Cspec0;
    float F0;
    TintColors(hitInfo, F0, Csheen, Cspec0);

    // Model weights
    float dielectricWt = (1.0 - hitInfo.metallic) * (1.0 - hitInfo.transmission);
    float metalWt = hitInfo.metallic;
    float glassWt = (1.0 - hitInfo.metallic) * hitInfo.transmission;

    // Lobe probabilities
    float schlickWt = SchlickWeight(V.z);

    float diffPr = dielectricWt * Luminance(hitInfo.baseColor);
    float dielectricPr = dielectricWt * Luminance(lerp(Cspec0, 1.0, schlickWt));
    float metalPr = metalWt * Luminance(lerp(hitInfo.baseColor, 1.0, schlickWt));
    float glassPr = glassWt;
    float clearCtPr = 0.25 * hitInfo.clearcoat;

    // Normalize probabilities
    float invTotalWt = 1.0 / (diffPr + dielectricPr + metalPr + glassPr + clearCtPr);
    diffPr *= invTotalWt;
    dielectricPr *= invTotalWt;
    metalPr *= invTotalWt;
    glassPr *= invTotalWt;
    clearCtPr *= invTotalWt;

    // CDF of the sampling probabilities
    float cdf[5];
    cdf[0] = diffPr;
    cdf[1] = cdf[0] + dielectricPr;
    cdf[2] = cdf[1] + metalPr;
    cdf[3] = cdf[2] + glassPr;
    cdf[4] = cdf[3] + clearCtPr;

    // Sample a lobe based on its importance
    float r3 = RandomFloat(random);

    if (r3 < cdf[0]) // Diffuse
    {
        L = CosineSampleHemisphere(r1, r2);
    }
    else if (r3 < cdf[2]) // Dielectric + Metallic reflection
    {
        float3 H = SampleGGXVNDF(V, hitInfo.ax, hitInfo.ay, r1, r2);

        if (H.z < 0.0)
            H = -H;

        L = normalize(reflect(-V, H));
    }
    else if (r3 < cdf[3]) // Glass
    {
        float3 H = SampleGGXVNDF(V, hitInfo.ax, hitInfo.ay, r1, r2);
        float F = DielectricFresnel(abs(dot(V, H)), hitInfo.eta);

        if (H.z < 0.0)
            H = -H;

        // Rescale random number for reuse
        r3 = (r3 - cdf[2]) / (cdf[3] - cdf[2]);

        // Reflection
        if (r3 < F)
        {
            L = normalize(reflect(-V, H));
        }
        else // Transmission
        {
            L = normalize(refract(-V, H, hitInfo.eta));
        }
    }
    else // Clearcoat
    {
        float3 H = SampleGTR1(hitInfo.clearcoatRoughness, r1, r2);

        if (H.z < 0.0)
            H = -H;

        L = normalize(reflect(-V, H));
    }

    L = ToWorld(T, B, N, L);
    V = ToWorld(T, B, N, V);

    return EvaluateBRDF(hitInfo, V, N, L, pdf);
}

#endif // DISNEY_H