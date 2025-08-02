struct PixelReadbackConstants
{
	uint2    pixelPosition;
};

cbuffer c_PixelReadback : register(b0)
{
	PixelReadbackConstants g_PixelReadback;
};
 
Texture2D<uint> t_Source : register(t0);
RWBuffer<uint> u_Dest : register(u0);

[numthreads(1, 1, 1)]
void Main()
{
	u_Dest[0] = t_Source[g_PixelReadback.pixelPosition.xy];
}
