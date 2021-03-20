#pragma once

#include <DirectXMath.h>

namespace rendering {
    struct ConstantBuffer {
        DirectX::XMMATRIX _world;
        DirectX::XMMATRIX _view;
        DirectX::XMMATRIX _projection;

        DirectX::XMFLOAT4 _light_pos[3];
        DirectX::XMFLOAT4 _light_color[3];
        DirectX::XMFLOAT4 _light_attenuation[3];
        float _light_intensity[9];

        float _adapted_log_luminance;
    };
}
