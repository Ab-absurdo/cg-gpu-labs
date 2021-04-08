#pragma once

#include <DirectXMath.h>

namespace rendering {

    const size_t N_LIGHTS = 3;

    struct GeometryOperatorsCB {
        DirectX::XMMATRIX _world;
        DirectX::XMMATRIX _world_normals;
        DirectX::XMMATRIX _view;
        DirectX::XMMATRIX _projection;
    };

    struct SurfacePropsCB {
        DirectX::XMFLOAT4 _base_color;
    };

    struct LightsCB {
        DirectX::XMFLOAT4 _ambient_light;
        DirectX::XMFLOAT4 _light_pos[N_LIGHTS];
        DirectX::XMFLOAT4 _light_color[N_LIGHTS];
        DirectX::XMFLOAT4 _light_attenuation[N_LIGHTS];
        float _light_intensity[3 * N_LIGHTS];
    };

    struct AdaptationCB {
        float _adapted_log_luminance;
    };
}
