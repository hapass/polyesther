//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj; 
};

SamplerState g_sampler : register(s0);
Texture2D g_texture : register(t0);

struct VertexIn
{
    float3 PosL  : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
	
    // Transform to homogeneous clip space.
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	
    // Just pass vertex texture coords into the pixel shader.
    vout.TexCoord = vin.TexCoord;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return g_texture.Sample(g_sampler, pin.TexCoord);
}


