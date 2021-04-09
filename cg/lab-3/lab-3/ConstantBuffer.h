#pragma once

#include <DirectXMath.h>

namespace rendering {

    const size_t N_LIGHTS = 1;

    struct GeometryOperatorsCB {
        DirectX::XMMATRIX _world;
        DirectX::XMMATRIX _world_normals;
        DirectX::XMMATRIX _view;
        DirectX::XMMATRIX _projection;
        DirectX::XMFLOAT4 _camera_pos;
    };

    __declspec(align(16))
    struct SurfacePropsCB {
        DirectX::XMFLOAT4 _base_color;
        float _roughness;
        float _metalness;
    };

    __declspec(align(16))
    struct LightsCB {
        DirectX::XMFLOAT4 _ambient_light;
        DirectX::XMFLOAT4 _light_pos[N_LIGHTS];
        DirectX::XMFLOAT4 _light_color[N_LIGHTS];
        DirectX::XMFLOAT4 _light_attenuation[N_LIGHTS];
        float _light_intensity[3 * N_LIGHTS];
    };

    __declspec(align(16))
    struct AdaptationCB {
        float _adapted_log_luminance;
    };
}
