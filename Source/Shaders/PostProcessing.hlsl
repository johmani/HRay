#include "Base.hlsli"

struct PostProssingInfo
{
    float exposure;
    float gamma;
};

RWTexture2D<float4> HDRColor : register(u0);
RWTexture2D<float4> LDRColor : register(u1);

ConstantBuffer<PostProssingInfo> postProssingInfo : register(b0);


[numthreads(8, 8, 1)]
void Main(uint2 id : SV_DispatchThreadID)
{
    //uint width, height;
    //renderTarget.GetDimensions(width, height);
    //float2 uv = float2(id) / float2(width, height) * 2.0 -1.0;
    //uv.y = - uv.y;
	//renderTarget[id] = float4(uv, 0, 1);
}
