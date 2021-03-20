#include "PointLight.h"

#include <DirectXColors.h>

namespace rendering {
    PointLight::PointLight() {
        _pos = { 0.0f, 0.0f, 0.0f, 0.0f };
        _color = (DirectX::XMFLOAT4)DirectX::Colors::White;
    }

    float PointLight::getIntensity() {
        return _intensities[_current_index];
    }

    void PointLight::changeIntensity() {
        _current_index = (_current_index + 1) % 3;
    }
}
