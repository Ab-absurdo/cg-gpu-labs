#pragma once

#include <directxmath.h>

class PointLight
{
public:
    PointLight();

    float getIntensity();
    void changeIntensity();

    DirectX::XMFLOAT4 _pos;
    DirectX::XMFLOAT4 _color;
    float _const_att = 0.1f, _lin_att = 1.0f, _exp_att = 1.0f;

private:
    float _intensities[3] = { 1.0f, 10.0f, 100.0f };
    size_t _current_index = 0;
};
