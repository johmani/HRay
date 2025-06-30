#ifndef UTILS_H
#define UTILS_H

#ifdef SPIRV
    #define VK_PUSH_CONSTANT [[vk::push_constant]]
    #define VK_BINDING(reg,dset) [[vk::binding(reg,dset)]]
#else
    #define VK_PUSH_CONSTANT
    #define VK_BINDING(reg,dset) 
#endif

#define INSTANCE_MASK_OPAQUE                1
#define INSTANCE_MASK_PARTICLE_GEOMETRY     2
#define INSTANCE_MASK_INTERSECTION_PARTICLE 4

static const uint c_Invalid = ~0u;
static const uint c_SizeOfTriangleIndices = 12;
static const uint c_SizeOfPosition = 12;
static const uint c_SizeOfNormal = 4;
static const uint c_SizeOfTexcoord = 8;
static const uint c_SizeOfJointIndices = 8;
static const uint c_SizeOfJointWeights = 16;

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


#endif // UTILS_H