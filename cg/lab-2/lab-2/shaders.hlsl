Texture2D txDiffuse : register(t0);

SamplerState samLinear : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;

    float4 LightPos[3];
    float4 LightColor[3];
    float4 LightAttenuation[3];
    float LightIntensity[3];
}

/* vertex attributes go here to input to the vertex shader */
struct vs_in {
    float4 position_local : POS;
    float3 normal_local : NOR;
    float2 texture_local : TEX;
};

/* outputs from vertex shader go here. can be interpolated to pixel shader */
struct vs_out {
    float4 position_clip : SV_POSITION; // required output of VS
    float4 position_world : TEXCOORD0;
    float3 normal_world : TEXCOORD1;
    float2 tex : TEXCOORD2;
};

vs_out vs_main(vs_in input) {
    vs_out output = (vs_out)0; // zero the memory first
    output.position_world = mul(input.position_local, World);
    output.normal_world = mul(float4(input.normal_local, 1), World).xyz;

    output.position_clip = mul(output.position_world, View);
    output.position_clip = mul(output.position_clip, Projection);
    output.tex = input.texture_local;
    return output;
}

float4 ps_main(vs_out input) : SV_TARGET{
    float4 color = 0;
    float3 light_dir;
    float dist, att, deg = 2.0f;
    for (int i = 0; i < 3; i++)
    {
        light_dir = LightPos[i].xyz - input.position_world.xyz;
        dist = length(light_dir);
        att = LightAttenuation[i].x + LightAttenuation[i].y * dist + LightAttenuation[i].z * dist * dist;
        color += pow(dot(light_dir / dist, input.normal_world), deg) / att * LightIntensity[i] * LightColor[i];
    }
    return txDiffuse.Sample(samLinear, input.tex) * color;
}
