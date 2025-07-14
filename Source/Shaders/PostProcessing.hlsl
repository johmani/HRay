#include "Base.hlsli"


RWTexture2D<float4> renderTarget : register(u0);

[numthreads(8, 8, 1)]
void Main(uint2 id : SV_DispatchThreadID)
{
    //uint width, height;
    //renderTarget.GetDimensions(width, height);
    //float2 uv = float2(id) / float2(width, height) * 2.0 -1.0;
    //uv.y = - uv.y;
	//renderTarget[id] = float4(uv, 0, 1);
}
