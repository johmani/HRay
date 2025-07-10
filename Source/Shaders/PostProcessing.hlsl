#include "Base.hlsli"

struct Constants
{
    uint frameIndex;
};

RWTexture2D<float4> prevFrame : register(u0);
RWTexture2D<float4> mainFrame : register(u1);
VK_PUSH_CONSTANT ConstantBuffer<Constants> constants : register(b0);

[numthreads(8, 8, 1)]
void Main(uint2 id : SV_DispatchThreadID)
{
    float3 c0 = SRGBToLinear(prevFrame[id.xy].rgb);
    float3 c1 = SRGBToLinear(mainFrame[id.xy].rgb);

	float t = 1.0 / (constants.frameIndex + 1);
    mainFrame[id.xy] = float4(LinearToSRGB(lerp(c0, c1, t)), 1);
}
