static const float PI = 3.14159265f;
static const float EPSILON = 1e-3f;
static const int N_LIGHTS = 1;

static const int N1 = 800;
static const int N2 = 200;

Texture2D _texture_2d : register(t0);

TextureCube _texture_cube : register(t0);

SamplerState _sam_state : register(s0);

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
    float _roughness;   // alpha
    float _metalness;
};

cbuffer Lights : register(b2)
{
    float4 _light_pos[N_LIGHTS];
    float4 _light_color[N_LIGHTS];
    float4 _light_attenuation[N_LIGHTS];
    float _light_intensity[N_LIGHTS];
};

cbuffer Adaptation : register(b3) {
    float _exposure_scale;
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

float SchlickGGX(float3 n, float3 v, float k) {
    const float dot_multiplier = saturate(dot(n, v));
    return dot_multiplier / (dot_multiplier * (1.0f - k) + k);
}

float geometryFunction(float3 normal, float3 dir)
{
    const float k = (_roughness + 1.0f) * (_roughness + 1.0f) / 8.0f;
    return SchlickGGX(normal, dir, k);
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

float3 fresnelFunctionAmbient(float3 camera_dir, float3 normal)
{
    const float3 F0_noncond = 0.04f;
    const float3 F0 = (1.0 - _metalness) * F0_noncond + _metalness * _base_color.rgb;
    const float dot_multiplier = saturate(dot(camera_dir, normal));
    return F0 + (max(1.0f - _roughness, F0) - F0) * pow(1.0f - dot_multiplier, 5);
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

float3 ambient(float3 camera_dir, float3 normal)
{
    float3 F = fresnelFunctionAmbient(camera_dir, normal);
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - _metalness;
    float3 irradiance = _texture_cube.Sample(_sam_state, normal).rgb;
    float3 diffuse = irradiance * _base_color.xyz;
    return kD * diffuse;
}

float4 psLambert(VsOut input) : SV_TARGET{
    const float3 normal = normalize(input._normal_world);
    float3 color = _base_color.rgb;
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
    float3 color = _base_color.rgb;
    for (int i = 0; i < N_LIGHTS; i++)
    {
        const float3 light_dir = normalize(_light_pos[i].xyz - pos);
        const float3 halfway = normalize(camera_dir + light_dir);
        const float3 radiance = projectedRadiance(i, pos, normal);
        color += radiance * brdf(normal, light_dir, camera_dir);
    }
    color += ambient(camera_dir, normal);
    return float4(color, _base_color.a);
}


VsCopyOut vsCopyMain(uint input : SV_VERTEXID) {
    VsCopyOut output = (VsCopyOut)0;
    output._tex = float2(input & 1, input >> 1);
    output._position = float4((output._tex.x - 0.5f) * 2, -(output._tex.y - 0.5f) * 2, 0, 1);
    return output;
}

float4 psCopyMain(VsCopyOut input) : SV_TARGET {
    return _texture_2d.Sample(_sam_state, input._tex);
}

float4 psLogLuminanceMain(VsCopyOut input) : SV_TARGET {
    float4 p = _texture_2d.Sample(_sam_state, input._tex);
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
    float e = exposure() * _exposure_scale;
    float3 curr = uncharted2Tonemap(e * color);
    float3 white_scale = 1.0f / uncharted2Tonemap(w);
    return curr * white_scale;
}

float4 psToneMappingMain(VsCopyOut input) : SV_TARGET {
    float4 color = _texture_2d.Sample(_sam_state, input._tex);
    return float4(pow(tonemapFilmic(color.xyz), 1 / 2.2), color.a);
}

float4 psCubeMap(VsOut input) : SV_TARGET{
    float3 n = normalize(input._position_world.xyz);
    float u = 1.0 - atan2(n.z, n.x) / (2 * PI);
    float v = 0.5 - asin(n.y) / PI;
    return _texture_2d.Sample(_sam_state, float2(u, v));
}

float4 psIrradianceMap(VsOut input) : SV_TARGET{
    float3 normal = normalize(input._position_world.xyz);
    // По умолчанию, "вперед" - это вперед, кроме случая, когда сама нормаль
    // направлена вперед или назад, тогда "вперед" - это вправо =)
    float3 dir = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(dir, normal));
    float3 bitangent = cross(normal, tangent);
    float3 irradiance = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < N1; i++) {
        for (int j = 0; j < N2; j++) {
            float phi = i * (2 * PI / N1);
            float theta = j * (PI / 2 / N2);
            // Перевод сферических координат в декартовы (в касательном пространстве)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // ... и из касательного пространства в мировое
            float3 sampleVec = tangentSample.x * tangent + tangentSample.y * bitangent + tangentSample.z * normal;
            irradiance += _texture_cube.Sample(_sam_state, sampleVec).rgb * cos(theta) * sin(theta);
        }
    }
    irradiance = PI * irradiance / (N1 * N2);
    return float4(irradiance, 1);
}

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 norm, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    // Перевод сферических координат в декартовы (в касательном пространстве)
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.z = sin(phi) * sinTheta;
    H.y = cosTheta;
    // ... и из касательного пространства в мировое
    float3 up = abs(norm.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, norm));
    float3 bitangent = cross(norm, tangent);
    float3 sampleVec = tangent * H.x + bitangent * H.z + norm * H.y;
    return normalize(sampleVec);
}

float4 psPrefilteredColor(VsOut input) : SV_TARGET{
    float3 norm = normalize(input._position_world.xyz);
    float3 view = norm;
    float totalWeight = 0.0;
    float3 prefilteredColor = float3(0, 0, 0);
    static const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, norm, _roughness);
        float3 L = normalize(2.0 * dot(view, H) * H - view);
        float ndotl = max(dot(norm, L), 0.0);
        float ndoth = max(dot(norm, H), 0.0);
        float hdotv = max(dot(H, view), 0.0);
        float D = ndf(norm, H);
        float pdf = (D * ndoth / (4.0 * hdotv)) + 0.0001;
        float resolution = 512.0; // разрешение иcходной environment map
        float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
        float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
        float mipLevel = _roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
        if (ndotl > 0.0) {
            prefilteredColor += _texture_cube.SampleLevel(_sam_state, L, mipLevel).rgb * ndotl;
            totalWeight += ndotl;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;
    return float4(prefilteredColor, 1);
}

float GeometrySmith(float3 n, float3 v, float3 l, float roughness) {
    const float k = _roughness * _roughness / 2.0f;
    return SchlickGGX(n, v, k) * SchlickGGX(n, l, k);
}

float2 IntegrateBRDF(float NdotV, float roughness)
{
    // Вычисляем V по NdotV, считая, что N=(0,1,0)​
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.z = 0.0;
    V.y = NdotV;
    // Кэффициенты, которые хотим посчитать​
    float A = 0.0;
    float B = 0.0;
    float3 N = float3(0.0, 1.0, 0.0);
    // Генерация псевдослучайных H
    static const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(L.y, 0.0);
        float NdotH = max(H.y, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        if (NdotL > 0.0) {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return float2(A, B);
}

float4 psPreintegratedBRDF(VsOut input) : SV_TARGET{
    return float4(IntegrateBRDF(input._position_world.x, 1 - input._position_world.y), 0, 1);
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
    return _texture_cube.Sample(_sam_state, input._tex);
}
