
Texture2D uiLayerColor : register(t0);
Texture2DMS<float> uiLayerDepth : register(t1);

Texture2D sceneColor : register(t2);
Texture2D sceneDepth : register(t3);

RWTexture2D<float4> compositeTarget : register(u0);

float4 AlphaBlend(float4 src, float4 dst)
{
    float4 result;
    result.rgb = src.rgb * src.a + dst.rgb * (1.0 - src.a);
    result.a = src.a + dst.a * (1.0 - src.a);
    return result;
}

float ResolveUILayerDepth(uint2 coord)
{
    float sum = 0.0f;

    [unroll]
    for (uint i = 0; i < 8; ++i)
    {
        sum += uiLayerDepth.Load(coord, i);
    }

    return sum / 8.0f;
}

[numthreads(8, 8, 1)]
void Main(uint2 id : SV_DispatchThreadID)
{
    float4 sceneCol = sceneColor[id];
    float4 uiCol = uiLayerColor[id];

    float sceneD = sceneDepth[id].r;
    float uiD = ResolveUILayerDepth(id);

    float4 fg, bg;

    if (sceneD < uiD)
    {
        fg = sceneCol;
        bg = uiCol;
    }
    else
    {
        fg = uiCol;
        bg = sceneCol;
    }

    compositeTarget[id] = AlphaBlend(fg, bg);
}
