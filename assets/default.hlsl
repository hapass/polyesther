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
    float4 PosH  : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.PosH = float4(vin.PosL, 1.0f);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 pos_view = g_texture[0].Sample(g_sampler, pin.TexCoord);

    float4 normal_vec = g_texture[1].Sample(g_sampler, pin.TexCoord);
    float4 light_vec = normalize(gLightPos - pos_view);

    float4 light_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float ambientStrength = 0.1f;
    float specularStrength = 0.5f;
    float specularShininess = 32.0f;

    float4 diffuse = light_color * max(dot(normal_vec, light_vec), 0.0);
    float4 ambient = light_color * ambientStrength;

    // might be broken, since reflect should accept normal as a second param
    float specAmount = max(dot(normalize(pos_view), reflect(light_vec, normal_vec)), 0.0f);
    float4 specular = light_color * pow(specAmount, specularShininess) * specularStrength;

    float4 frag_color = g_texture[2].Sample(g_sampler, pin.TexCoord);
    float4 res = (diffuse + ambient + specular) * frag_color;
    res.w = 1.0f; // fix the alpha being affected by light, noticable only in tests, because in application we correct alpha manually when copying to backbuffer 
    return clamp(res, 0.0f, 1.0f);
}