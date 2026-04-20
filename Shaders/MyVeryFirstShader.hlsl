cbuffer CBPerObject : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    float4 Color;
    float4 LightDirAmbient;
};

Texture2D diffuseTex : register(t0);
SamplerState sampLinear : register(s0);

struct VS_IN
{
    float4 pos : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD0;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 tex : TEXCOORD0;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;
    float4 worldPos = mul(input.pos, World);
    float4 viewPos = mul(worldPos, View);
    output.pos = mul(viewPos, Projection);

    float3 N = normalize(mul(float4(input.normal, 0.0), World).xyz);
    float3 L = normalize(LightDirAmbient.xyz);
    float ndotl = saturate(dot(N, L));
    float amb = LightDirAmbient.w;
    float shade = amb + (1.0 - amb) * ndotl;
    output.color = Color * input.color * float4(shade, shade, shade, 1.0);
    output.tex = input.tex;
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float4 tex = diffuseTex.Sample(sampLinear, input.tex);
    return tex * input.color;
}
