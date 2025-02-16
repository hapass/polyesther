cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorldView;
    float4 gLightPos;
};

SamplerState g_sampler : register(s0);
Texture2D g_texture[] : register(t0);

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexCoord : TEXCOORD;
    float3 Normals : NORMALS;
    float3 Color : COLOR;
    int TexIndex : TEXINDEX;
};