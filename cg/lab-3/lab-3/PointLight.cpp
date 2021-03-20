#include "PointLight.h"

#include <DirectXColors.h>

namespace rendering {
    float PointLight::getIntensity() {
        return _intensities[_current_index];
    }

    void PointLight::changeIntensity() {
        _current_index = (_current_index + 1) % 3;
    }
}
