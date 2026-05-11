cbuffer CBPerObject : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    matrix LightViewProj;
    float4 Color;
    float4 LightDirAmbient;
    float4 ShadowParams; // x = enable, y = bias, z = texelSize, w = unused
};

Texture2D diffuseTex : register(t0);
Texture2D shadowMap : register(t1);
SamplerState sampLinear : register(s0);
SamplerComparisonState sampShadow : register(s1);

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
    float4 lightPos : TEXCOORD1;
    float3 normalW : TEXCOORD2;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;
    float4 worldPos = mul(input.pos, World);
    float4 viewPos = mul(worldPos, View);
    output.pos = mul(viewPos, Projection);
    output.lightPos = mul(worldPos, LightViewProj);
    output.normalW = mul(float4(input.normal, 0.0), World).xyz;
    output.color = Color * input.color;
    output.tex = input.tex;
    return output;
}

float ComputeShadowFactor(float4 lightClip, float ndotl)
{
    if (ShadowParams.x < 0.5)
        return 1.0;

    float3 ndc = lightClip.xyz / lightClip.w;
    float2 uv = ndc.xy * float2(0.5, -0.5) + 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ||
        ndc.z < 0.0 || ndc.z > 1.0)
        return 1.0;

    float bias = max(ShadowParams.y * (1.0 - ndotl), ShadowParams.y * 0.15);
    float depthRef = ndc.z - bias;

    float ts = ShadowParams.z;
    float sum = 0.0;
    [unroll]
    for (int j = -1; j <= 1; ++j)
    {
        [unroll]
        for (int i = -1; i <= 1; ++i)
        {
            float2 sampleUv = uv + float2(i, j) * ts;
            sum += shadowMap.SampleCmpLevelZero(sampShadow, sampleUv, depthRef);
        }
    }
    return sum / 9.0;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float3 N = normalize(input.normalW);
    float3 L = normalize(LightDirAmbient.xyz);
    float ndotl = saturate(dot(N, L));
    float shadow = ComputeShadowFactor(input.lightPos, ndotl);
    float amb = LightDirAmbient.w;
    float shade = amb + (1.0 - amb) * ndotl * shadow;

    float4 albedo = diffuseTex.Sample(sampLinear, input.tex);
    return albedo * input.color * float4(shade, shade, shade, 1.0);
}

float4 VSShadow(VS_IN input) : SV_Position
{
    float4 worldPos = mul(input.pos, World);
    return mul(worldPos, LightViewProj);
}
