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

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 PosView : POSITION;
    float2 TexCoord : TEXCOORD;
    float3 Normals : NORMALS;
    float3 Color : COLOR;
    int TexIndex : TEXINDEX;
};

struct GBuffer
{
    float4 PosView: SV_Target0;
    float4 Normal: SV_Target1;
    float4 Diffuse: SV_Target2;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
	
    // Transform to homogeneous clip space.
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);

    // Transform to view space.
    vout.PosView = mul(float4(vin.PosL, 1.0f), gWorldView);
    vout.Normals = mul(float4(vin.Normals, 0.0f), gWorldView).xyz;
	
    // Just pass vertex texture coords into the pixel shader.
    vout.TexCoord = vin.TexCoord;
    vout.TexIndex = vin.TexIndex;
    vout.Color = vin.Color;

    return vout;
}

GBuffer PS(VertexOut pin)
{
    GBuffer buf;
    buf.PosView = float4(pin.PosView.xyz, 1.0f);
    buf.Normal = normalize(float4(pin.Normals.xyz, 0.0f));
    buf.Diffuse = pin.TexIndex == -1 ? float4(pin.Color.xyz, 1.0) : g_texture[pin.TexIndex].Sample(g_sampler, float2(pin.TexCoord.x, 1.0 - pin.TexCoord.y));
    return buf;
}