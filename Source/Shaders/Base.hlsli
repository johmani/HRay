#ifndef UTILS_H
#define UTILS_H


#pragma region Constants


#define INSTANCE_MASK_OPAQUE                1
#define INSTANCE_MASK_PARTICLE_GEOMETRY     2
#define INSTANCE_MASK_INTERSECTION_PARTICLE 4

static const float c_RayPosNormalOffset = 0.001;
static const float c_PI = 3.14159265;
static const float c_2PI = 2.0f * c_PI;
static const float c_Inv_PI = 1.0 / c_PI;
static const float c_Inv_2PI = 1.0 / c_2PI;

static const uint c_Invalid = ~0u;
static const uint c_SizeOfTriangleIndices = 12;
static const uint c_SizeOfPosition = 12;
static const uint c_SizeOfNormal = 4;
static const uint c_SizeOfTexcoord = 8;
static const uint c_SizeOfJointIndices = 8;
static const uint c_SizeOfJointWeights = 16;

static const uint c_TonMapingType_None = 0;
static const uint c_TonMapingType_WhatEver = 1;
static const uint c_TonMapingType_ACES = 2;
static const uint c_TonMapingType_ACESFitted = 3;
static const uint c_TonMapingType_Filmic = 4;
static const uint c_TonMapingType_Reinhard = 5;

struct HitInfo
{
    float3 normal;
    float3 ffnormal;
    float3 tangent;
    float3 bitangent;
    float distance;

    float3 baseColor;
    float3 emissive;
    float metallic;
    float roughness;

    bool HasHit() { return distance < 1000; }
};

#pragma endregion
#pragma region Macros


#ifdef SPIRV
    #define VK_PUSH_CONSTANT [[vk::push_constant]]
    #define VK_BINDING(reg,dset) [[vk::binding(reg,dset)]]
#else
    #define VK_PUSH_CONSTANT
    #define VK_BINDING(reg,dset) 
#endif


#pragma endregion
#pragma region Utils


float4x4 inverse(float4x4 m)
{
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}

float2 Interpolate(float2 vertices[3], float3 bary)
{
    return vertices[0] * bary[0] + vertices[1] * bary[1] + vertices[2] * bary[2];
}

float3 Interpolate(float3 vertices[3], float3 bary)
{
    return vertices[0] * bary[0] + vertices[1] * bary[1] + vertices[2] * bary[2];
}

float4 Interpolate(float4 vertices[3], float3 bary)
{
    return vertices[0] * bary[0] + vertices[1] * bary[1] + vertices[2] * bary[2];
}

float Unpack_R8_SNORM(uint value)
{
    int signedValue = int(value << 24) >> 24;
    return clamp(float(signedValue) / 127.0, -1.0, 1.0);
}

float3 Unpack_RGB8_SNORM(uint value)
{
    return float3(
        Unpack_R8_SNORM(value),
        Unpack_R8_SNORM(value >> 8),
        Unpack_R8_SNORM(value >> 16)
    );
}

float4 Unpack_RGBA8_SNORM(uint value)
{
    return float4(
        Unpack_R8_SNORM(value),
        Unpack_R8_SNORM(value >> 8),
        Unpack_R8_SNORM(value >> 16),
        Unpack_R8_SNORM(value >> 24)
    );
}

float RandomFloat(inout uint state)
{
    state = state * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    result = (result >> 22) ^ result;
    return result / 4294967295.0;
}

float RandomFloatNormalDistribution(inout uint state)
{
    float theta = c_2PI * RandomFloat(state);
    float rho = sqrt(-2 * log(RandomFloat(state)));
    return rho * cos(theta);
}

float3 RandomDirection(inout uint state)
{
    float x = RandomFloatNormalDistribution(state);
    float y = RandomFloatNormalDistribution(state);
    float z = RandomFloatNormalDistribution(state);
    return normalize(float3(x, y, z));
}

float2 RandomPointInCircle(inout uint rngState)
{
    float angle = RandomFloat(rngState) * c_2PI;
    float2 pointOnCircle = float2(cos(angle), sin(angle));
    return pointOnCircle * sqrt(RandomFloat(rngState));
}

float2 CartesianToSphericalUnorm(float3 p)
{
    p = normalize(p);
    float2 sph;
    sph.x = acos(p.z) * c_PI;
    sph.y = atan2(-p.y, -p.x) * c_PI + 0.5f;
    return sph;
}

float3 SRGBToLinear(float3 c)
{
    return pow(c, 2.2);
}

float3 LinearToSRGB(float3 c)
{
    return pow(c, 1 / 2.2);
}

void GetCameraRightUp(float4x4 clipToWorld, out float3 camRight, out float3 camUp)
{
    float4 originCS = float4(0, 0, 0, 1);
    float4 rightCS = float4(1, 0, 0, 1);
    float4 upCS = float4(0, 1, 0, 1);

    float3 worldOrigin = mul(clipToWorld, originCS).xyz;
    float3 worldRight = mul(clipToWorld, rightCS).xyz;
    float3 worldUp = mul(clipToWorld, upCS).xyz;

    camRight = normalize(worldRight - worldOrigin);
    camUp = normalize(worldUp - worldOrigin);
}

float Luminance(float3 c)
{
    return 0.212671 * c.x + 0.715160 * c.y + 0.072169 * c.z;
}

float3 Tonemap(in float3 c, float limit)
{
    return c * 1.0 / (1.0 + Luminance(c) / limit);
}

float3 ACES(float3 color)
{
    return (color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14);
}

float3 Filmic(float3 color, float exposure = 2.0)
{
    float3 x = color * exposure;

    // Hable's curve
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;

    x = ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;

    // Normalize to white
    float whitePoint = 11.2;
    x *= 1.0 / (((whitePoint * (A * whitePoint + C * B) + D * E) / (whitePoint * (A * whitePoint + B) + D * F)) - E / F);
    return saturate(x);
}

float3 Reinhard(float3 color, float exposure)
{
    color *= exposure;
    return color / (1.0 + color);
}

// Baking Lab
// by MJP and David Neubelt
// http://mynameismjp.wordpress.com/
// All code licensed under the MIT license
// The code in this file was originally written by Stephen Hill (@self_shadow), who deserves all
// credit for coming up with this fit and implementing it. Buy him a beer next time you see him. :)

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
{
    { 0.59719, 0.35458, 0.04823 },
    { 0.07600, 0.90834, 0.01566 },
    { 0.02840, 0.13383, 0.83777 }
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367 },
    { -0.10208, 1.10813, -0.00605 },
    { -0.00327, -0.07276, 1.07602 }
};

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 ACESFitted(float3 color)
{
    color = mul(ACESInputMat, color);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);
    color = mul(ACESOutputMat, color);
    color = saturate(color);

    return color;
}


#pragma endregion


#endif // UTILS_H