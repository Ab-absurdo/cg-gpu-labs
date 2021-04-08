#define N_LIGHTS 3

Texture2D _tx_diffuse : register(t0);

SamplerState _sam_linear : register(s0);

cbuffer GeometryOperators : register(b0)
{
    matrix _world;
    matrix _world_normals;
    matrix _view;
    matrix _projection;
};

cbuffer SurfaceProps : register(b1)
{
    float4 _base_color; // albedo
};

cbuffer Lights : register(b2)
{
    float4 _ambient_light;
    float4 _light_pos[N_LIGHTS];
    float4 _light_color[N_LIGHTS];
    float4 _light_attenuation[N_LIGHTS];
    float _light_intensity[N_LIGHTS];
};

cbuffer Adaptation : register(b3) {

    float _adapted_log_luminance;
};

struct VsIn {
    float4 _position_local : POS;
    float3 _normal_local : NOR;
};

struct VsOut {
    float4 _position_projected : SV_POSITION;
    float4 _position_world : TEXCOORD0;
    float3 _normal_world : TEXCOORD1;
  
};

struct VsCopyOut {
    float4 _position : SV_POSITION;
    float2 _tex : TEXCOORD;
};

VsOut vsMain(VsIn input) {
    VsOut output = (VsOut)0;
    output._position_world = mul(input._position_local, _world);
    output._normal_world = mul(float4(input._normal_local, 1), _world_normals).xyz;

    output._position_projected = mul(output._position_world, _view);
    output._position_projected = mul(output._position_projected, _projection);

    return output;
}

float3 projected_radiance(int index, float3 pos, float3 nor) // L_i * (l, n)
{
    const float deg = 5.0f;
    const float3 light_dir = _light_pos[index].xyz - pos;
    const float dist = length(light_dir);
    const float dot_multiplier = pow(saturate(dot(light_dir / dist, nor)), deg);
    float att = _light_attenuation[index].x + _light_attenuation[index].y * dist;
    att +=  _light_attenuation[index].z * dist * dist;
    return _light_intensity[index] * dot_multiplier / att * _light_color[index].rgb;
}

float4 psLambert(VsOut input) : SV_TARGET {
    float3 color = _base_color.rgb + _ambient_light.rgb;
    for (int i = 0; i < N_LIGHTS; i++)
    {
        color += projected_radiance(i, input._position_world, input._normal_world);
    }
    return float4(color, 1.0f);
}

VsCopyOut vsCopyMain(uint input : SV_VERTEXID) {
    VsCopyOut output = (VsCopyOut)0;
    output._tex = float2(input & 1, input >> 1);
    output._position = float4((output._tex.x - 0.5f) * 2, -(output._tex.y - 0.5f) * 2, 0, 1);
    return output;
}

float4 psCopyMain(VsCopyOut input) : SV_TARGET {
    return _tx_diffuse.Sample(_sam_linear, input._tex);
}

float4 psLogLuminanceMain(VsCopyOut input) : SV_TARGET {
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

float4 psToneMappingMain(VsCopyOut input) : SV_TARGET {
    float4 color = _tx_diffuse.Sample(_sam_linear, input._tex);
    return float4(pow(tonemapFilmic(color.xyz), 1 / 2.2), color.a);
}
