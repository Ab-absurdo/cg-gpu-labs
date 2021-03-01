cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;

    float4 LightPos[3];
    float4 LightColor[3];
    float LightIntensity[3];
}

/* vertex attributes go here to input to the vertex shader */
struct vs_in {
    float4 position_local : POS;
    float4 color_local : COL;
    float3 normal_local : NOR;
};

/* outputs from vertex shader go here. can be interpolated to pixel shader */
struct vs_out {
    float4 position_clip : SV_POSITION; // required output of VS
    float4 color : COLOR0;
    float4 position_world : TEXCOORD0;
    float3 normal_world : TEXCOORD1;
};

vs_out vs_main(vs_in input) {
    vs_out output = (vs_out)0; // zero the memory first
    output.position_world = mul(input.position_local, World);
    output.normal_world = mul(float4(input.normal_local, 1), World).xyz;

    output.position_clip = mul(output.position_world, View);
    output.position_clip = mul(output.position_clip, Projection);
    output.color = input.color_local;
    return output;
}

float4 ps_main(vs_out input) : SV_TARGET{
    float4 color = input.color;
    float3 light_dir;
    float deg = 10.0f;
    for (int i = 0; i < 3; i++)
    {
        light_dir = normalize(LightPos[i].xyz - input.position_world.xyz);
        color += pow(dot(light_dir, input.normal_world), deg) * LightIntensity[i] * LightColor[i];
    }
    return color;
}
