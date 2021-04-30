#pragma once

#include <DirectXMath.h>

namespace rendering {
    class PointLight {
    public:
        float getIntensity();
        void changeIntensity();

        DirectX::XMFLOAT4 _pos;
        DirectX::XMFLOAT4 _color;

        float _const_att = 0.1f;
        float _lin_att = 1.0f;
        float _exp_att = 1.0f;

    private:
        float _intensities[3] = { 1.0f, 10.0f, 100.0f };
        size_t _current_index = 0;
    };
}
