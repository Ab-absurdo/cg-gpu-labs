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

    float AverageLogLuminance;
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
    float dist, att, deg = 5.0f;
    for (int i = 0; i < 3; i++)
    {
        light_dir = LightPos[i].xyz - input.position_world.xyz;
        dist = length(light_dir);
        att = LightAttenuation[i].x + LightAttenuation[i].y * dist + LightAttenuation[i].z * dist * dist;
        color += pow(dot(light_dir / dist, input.normal_world), deg) / att * LightIntensity[i] * LightColor[i];
    }
    return txDiffuse.Sample(samLinear, input.tex) * color;
}

vs_out vs_copy_main(uint input : SV_VERTEXID) {
    vs_out output = (vs_out)0; // zero the memory first
    output.tex = float2(input & 1, input >> 1);
    output.position_clip = float4((output.tex.x - 0.5f) * 2, -(output.tex.y - 0.5f) * 2, 0, 1);
    return output;
}

float4 ps_copy_main(vs_out input) : SV_TARGET{
    return txDiffuse.Sample(samLinear, input.tex);
}

float4 ps_log_luminance_main(vs_out input) : SV_TARGET{
    float4 p = txDiffuse.Sample(samLinear, input.tex);
    float L = 0.2126 * p.r + 0.7151 * p.g + 0.0722 * p.b;
    return log(L + 1);
}

static const float A = 0.1;  // Shoulder Strength
static const float B = 0.50; // Linear Strength
static const float C = 0.1;  // Linear Angle
static const float D = 0.20; // Toe Strength
static const float E = 0.02; // Toe Numerator
static const float F = 0.30; // Toe Denominator
                             // Note: E/F = Toe Angle
static const float W = 11.2; // Linear White Point Value

float3 Uncharted2Tonemap(float3 x) {
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float KeyValue(float L) {
    return 1.03 - 2 / (2 + log10(L + 1));
}

float exposure() {
    float L = exp(AverageLogLuminance) - 1;
    return KeyValue(L) / L;
}

float3 TonemapFilmic(float3 color)
{
    float E = exposure();
    float3 curr = Uncharted2Tonemap(E * color);
    float3 whiteScale = 1.0f / Uncharted2Tonemap(W);
    return curr * whiteScale;
}

float4 ps_tone_mapping_main(vs_out input) : SV_TARGET{
    float4 color = txDiffuse.Sample(samLinear, input.tex);
    return float4(pow(TonemapFilmic(color.xyz), 1 / 2.2), color.a);
}
