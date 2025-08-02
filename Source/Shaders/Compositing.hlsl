
Texture2D uiLayerColor : register(t0);
Texture2DMS<float> uiLayerDepth : register(t1);
Texture2DMS<uint> uiLayerID : register(t2);

Texture2D sceneColor : register(t3);
Texture2D sceneDepth : register(t4);
Texture2D<uint> sceneID : register(t5);

RWTexture2D<float4> compositeTarget : register(u0);
RWTexture2D<uint> idTarget : register(u1);

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

    uint entityID;
    float4 fg, bg;

    if (sceneDepth[id].r < ResolveUILayerDepth(id))
    {
        fg = sceneCol;
        bg = uiCol;
        entityID = sceneID[id];
    }
    else
    {
        fg = uiCol;
        bg = sceneCol;
        entityID = uiLayerID.Load(id, 0);
    }

    compositeTarget[id] = AlphaBlend(fg, bg);
    idTarget[id] = entityID;
}
