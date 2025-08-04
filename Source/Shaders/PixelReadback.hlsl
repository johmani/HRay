struct PixelReadbackConstants
{
	uint2 pixelPosition;
};

cbuffer c_PixelReadback : register(b0)
{
	PixelReadbackConstants pixelReadback;
};
 
Texture2D<uint> source : register(t0);
RWBuffer<uint> dest : register(u0);

[numthreads(1, 1, 1)]
void Main()
{
	dest[0] = source[pixelReadback.pixelPosition.xy];
}
