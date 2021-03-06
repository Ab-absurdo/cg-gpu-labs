static const float PI = 3.14159265f;
static const float EPSILON = 1e-3f;
static const int N_LIGHTS = 1;

Texture2D _tx_diffuse : register(t0);

TextureCube _sky_map : register(t0);

SamplerState _sam_linear : register(s0);

cbuffer GeometryOperators : register(b0)
{
    matrix _world;
    matrix _world_normals;
    matrix _view;
    matrix _projection;
    float4 _camera_pos;
};

cbuffer SurfaceProps : register(b1)
{
    float4 _base_color; // albedo
    float _roughness;   // aplpha
    float _metalness;
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

struct VsSkymapOut {
    float4 _pos : SV_POSITION;
    float3 _tex : TEXCOORD;
};

VsOut vsMain(VsIn input) {
    VsOut output = (VsOut)0;
    output._position_world = mul(input._position_local, _world);
    output._normal_world = normalize(mul(float4(input._normal_local, 1), _world_normals).xyz);

    output._position_projected = mul(output._position_world, _view);
    output._position_projected = mul(output._position_projected, _projection);

    return output;
}

float3 projectedRadiance(int index, float3 pos, float3 normal) // L_i * (l, n)
{
    const float deg = 1.0f;
    const float3 light_dir = _light_pos[index].xyz - pos;
    const float dist = length(light_dir);
    const float dot_multiplier = pow(saturate(dot(light_dir / dist, normal)), deg);
    float att = _light_attenuation[index].x + _light_attenuation[index].y * dist * dist;
    return _light_intensity[index] * dot_multiplier / att * _light_color[index].rgb;
}

float ndf(float3 normal, float3 halfway) { // D (Trowbridge-Reitz GGX)
    const float roughness_squared = clamp(_roughness * _roughness, EPSILON, 1.0f);
    const float n_dot_h = saturate(dot(normal, halfway));
    return roughness_squared / PI / pow(n_dot_h * n_dot_h * (roughness_squared - 1.0f) + 1.0f, 2);
}

float geometryFunction(float3 normal, float3 dir)
{
    const float k = (_roughness + 1.0f) * (_roughness + 1.0f) / 8.0f;
    const float dot_multiplier = saturate(dot(normal, dir));
    return dot_multiplier / (dot_multiplier * (1.0f - k) + k);
}

float geometryFunction2dir(float3 normal, float3 light_dir, float3 camera_dir)
{
    return geometryFunction(normal, light_dir) * geometryFunction(normal, camera_dir);
}

float3 fresnelFunction(float3 camera_dir, float3 halfway)
{
    const float3 F0_noncond = 0.04f;
    const float3 F0 = (1.0 - _metalness) * F0_noncond + _metalness * _base_color.rgb;
    const float dot_multiplier = saturate(dot(camera_dir, halfway));
    return F0 + (1.0f - F0) * pow(1.0f - dot_multiplier, 5);
}

float3 brdf(float3 normal, float3 light_dir, float3 camera_dir)
{
    const float3 halfway = normalize(camera_dir + light_dir);
    const float l_dot_n = saturate(dot(light_dir, normal));
    const float v_dot_n = saturate(dot(camera_dir, normal));
    const float D = ndf(normal, halfway);
    const float G = geometryFunction2dir(normal, light_dir, camera_dir);
    const float3 F = fresnelFunction(camera_dir, halfway);

    const float3 f_lamb = (1.0f - F) * (1.0f - _metalness) * _base_color.rgb / PI;
    const float3 f_ct = D * F * G / (4.0f * l_dot_n * v_dot_n + EPSILON);
    return f_lamb + f_ct;
}


float4 psLambert(VsOut input) : SV_TARGET{
    const float3 normal = normalize(input._normal_world);
    float3 color = _base_color.rgb + _ambient_light.rgb;
    for (int i = 0; i < N_LIGHTS; i++)
    {
        color += projectedRadiance(i, input._position_world.xyz, normal);
    }
    return float4(color, _base_color.a);
}

float4 psNDF(VsOut input) : SV_TARGET {
    const float3 pos = input._position_world.xyz;
    const float3 normal = normalize(input._normal_world);
    const float3 camera_dir = normalize(_camera_pos.xyz - pos);
    float color_grayscale = 0.0f;
    for (int i = 0; i < N_LIGHTS; i++)
    {
        const float3 light_dir = normalize(_light_pos[i].xyz - pos);
        const float3 halfway = normalize(camera_dir + light_dir);
        color_grayscale += ndf(normal, halfway);
    }
    return color_grayscale;
}

float4 psGeometry(VsOut input) : SV_TARGET{
    const float3 pos = input._position_world.xyz;
    const float3 normal = normalize(input._normal_world);
    const float3 camera_dir = normalize(_camera_pos.xyz - pos);
    float color_grayscale = 0.0f;
    for (int i = 0; i < N_LIGHTS; i++)
    {
        const float3 light_dir = normalize(_light_pos[i].xyz - pos);
        color_grayscale += geometryFunction2dir(normal, light_dir, camera_dir);
    }
    return color_grayscale;
}

float4 psFresnel(VsOut input) : SV_TARGET{
    const float3 pos = input._position_world.xyz;
    const float3 normal = normalize(input._normal_world);
    const float3 camera_dir = normalize(_camera_pos.xyz - pos);
    float3 color = 0.0f;
    for (int i = 0; i < N_LIGHTS; i++)
    {
        const float3 light_dir = normalize(_light_pos[i].xyz - pos);
        const float3 halfway = normalize(camera_dir + light_dir);
        color += fresnelFunction(camera_dir, halfway) * (dot(light_dir, normal) > 0.0f);
    }
    return float4(color, _base_color.a);
}

float4 psPBR(VsOut input) : SV_TARGET{
    const float3 pos = input._position_world.xyz;
    const float3 normal = normalize(input._normal_world);
    const float3 camera_dir = normalize(_camera_pos.xyz - pos);
    float3 color = _base_color.rgb + _ambient_light.rgb;
    for (int i = 0; i < N_LIGHTS; i++)
    {
        const float3 light_dir = normalize(_light_pos[i].xyz - pos);
        const float3 halfway = normalize(camera_dir + light_dir);
        const float3 radiance = projectedRadiance(i, pos, normal);
        color += radiance * brdf(normal, light_dir, camera_dir);
    }
    return float4(color, _base_color.a);
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

VsSkymapOut vsSkymap(VsIn input) {
    VsSkymapOut output = (VsSkymapOut)0;
    output._pos = mul(input._position_local, _world);
    output._pos = mul(output._pos, _view);
    output._pos = mul(output._pos, _projection);
    output._pos = output._pos.xyww;
    output._tex = input._position_local.xyz;
    return output;
}

float4 psSkymap(VsSkymapOut input) : SV_TARGET {
    return _sky_map.Sample(_sam_linear, input._tex);
}
