Texture2D tex : register(t0);
SamplerState samp : register(s0);

struct VS_INPUT
{
    float3 pos : POSITION;
    float2 tex : TEXCOORD;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_Target
{
    return tex.Sample(samp, input.tex);
}