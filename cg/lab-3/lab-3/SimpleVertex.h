#pragma once

#include <DirectXMath.h>

namespace rendering {
    struct SimpleVertex {
        DirectX::XMFLOAT3 _pos;
        DirectX::XMFLOAT3 _nor;
        DirectX::XMFLOAT2 _tex;
    };
}
