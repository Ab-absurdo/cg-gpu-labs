#include "PointLight.h"

#include <directxcolors.h>

using namespace DirectX;

PointLight::PointLight()
{
    _pos = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    _color = (XMFLOAT4)Colors::White;
}

float PointLight::getIntensity() { return _intensities[_current_index]; }

void PointLight::changeIntensity() { _current_index = (_current_index + 1) % 3; }
