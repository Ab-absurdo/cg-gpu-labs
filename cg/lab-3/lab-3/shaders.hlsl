Texture2D _tx_diffuse : register(t0);

SamplerState _sam_linear : register(s0);

cbuffer ConstantBuffer : register(b0) {
    matrix _world;
    matrix _view;
    matrix _projection;

    float4 _ambient_light;
    float4 _light_pos[3];
    float4 _light_color[3];
    float4 _light_attenuation[3];
    float _light_intensity[3];

    float _adapted_log_luminance;
}

struct VsIn {
    float4 _position_local : POS;
    float3 _normal_local : NOR;
    float2 _texture_local : TEX;
};

struct VsOut {
    float4 _position_clip : SV_POSITION;
    float4 _position_world : TEXCOORD0;
    float3 _normal_world : TEXCOORD1;
    float2 _tex : TEXCOORD2;
};

VsOut vsMain(VsIn input) {
    VsOut output = (VsOut)0;
    output._position_world = mul(input._position_local, _world);
    output._normal_world = mul(float4(input._normal_local, 1), _world).xyz;

    output._position_clip = mul(output._position_world, _view);
    output._position_clip = mul(output._position_clip, _projection);
    output._tex = input._texture_local;

    return output;
}

float4 psMain(VsOut input) : SV_TARGET {
    float deg = 5.0f;
    float4 color = _ambient_light;
    for (int i = 0; i < 3; i++)
    {
        float3 light_dir = _light_pos[i].xyz - input._position_world.xyz;
        float dist = length(light_dir);
        float att = _light_attenuation[i].x + _light_attenuation[i].y * dist + _light_attenuation[i].z * dist * dist;
        float dot_multiplier = pow(clamp(dot(light_dir / dist, input._normal_world), 0.0f, 1.0f), deg);
        color += dot_multiplier / att * _light_intensity[i] * _light_color[i];
    }
    return _tx_diffuse.Sample(_sam_linear, input._tex) * color;
}

VsOut vsCopyMain(uint input : SV_VERTEXID) {
    VsOut output = (VsOut)0;
    output._tex = float2(input & 1, input >> 1);
    output._position_clip = float4((output._tex.x - 0.5f) * 2, -(output._tex.y - 0.5f) * 2, 0, 1);
    return output;
}

float4 psCopyMain(VsOut input) : SV_TARGET {
    return _tx_diffuse.Sample(_sam_linear, input._tex);
}

float4 psLogLuminanceMain(VsOut input) : SV_TARGET {
    float4 p = _tx_diffuse.Sample(_sam_linear, input._tex);
    float l = 0.2126 * p.r + 0.7151 * p.g + 0.0722 * p.b;
    return log(l + 1);
}

static const float a = 0.1;  // Shoulder Strength
static const float b = 0.50; // Linear Strength
static const float c = 0.1;  // Linear Angle
static const float d = 0.20; // Toe Strength
static const float e = 0.02; // Toe Numerator
static const float f = 0.30; // Toe Denominator
                             // Note: E/F = Toe Angle
static const float w = 11.2; // Linear White Point Value

float3 uncharted2Tonemap(float3 x) {
    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

float keyValue(float l) {
    return 1.03 - 2 / (2 + log10(l + 1));
}

float exposure() {
    float L = exp(_adapted_log_luminance) - 1;
    return keyValue(L) / L;
}

float3 tonemapFilmic(float3 color)
{
    float e = exposure();
    float3 curr = uncharted2Tonemap(e * color);
    float3 white_scale = 1.0f / uncharted2Tonemap(w);
    return curr * white_scale;
}

float4 psToneMappingMain(VsOut input) : SV_TARGET {
    float4 color = _tx_diffuse.Sample(_sam_linear, input._tex);
    return float4(pow(tonemapFilmic(color.xyz), 1 / 2.2), color.a);
}
