#pragma once

#include <DirectXMath.h>

namespace rendering {
    struct WorldBorders {
        DirectX::XMVECTOR _min;
        DirectX::XMVECTOR _max;
    };
}
