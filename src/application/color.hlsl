//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorldView;
    float4 gLightPos;
};

SamplerState g_sampler : register(s0);
Texture2D g_texture : register(t0);

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexCoord : TEXCOORD;
    float3 Normals : NORMALS;
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;
    float4 PosView: POSITION;
    float2 TexCoord : TEXCOORD;
    float3 Normals : NORMALS;
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
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 pos_view = { pin.PosView.xyz, 1.0f };
    float4 normal_raw = { pin.Normals.xyz, 0.0f };
    float4 normal_vec = normalize(normal_raw);
    float4 light_vec = normalize(gLightPos - pos_view);

    float4 light_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float ambientStrength = 0.1f;
    float specularStrength = 0.5f;
    float specularShininess = 32.0f;

    float4 diffuse = light_color * max(dot(normal_vec, light_vec), 0.0);
    float4 ambient = light_color * ambientStrength;

    // might be broken, since reflect should accept normal as a second param
    float specAmount = max(dot(normalize(pos_view), reflect(light_vec, normal_vec)), 0.0f);
    float specular = light_color * pow(specAmount, specularShininess) * specularStrength;

    float4 frag_color = g_texture.Sample(g_sampler, pin.TexCoord);
    float4 res = (diffuse + ambient + specular) * frag_color;
    return clamp(res, 0.0f, 1.0f);
}


