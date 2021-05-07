#pragma once

#include <DirectXMath.h>

#include "WorldBorders.h"

namespace rendering {
    class Camera {
    public:
        Camera(const DirectX::XMVECTOR& pos = { 0.0f, 0.0f, 0.0f }, const DirectX::XMVECTOR& dir = { 0.0f, 0.0f, 1.0f }, const DirectX::XMVECTOR& up = { 0.0f, 1.0f, 0.0f });

        DirectX::XMMATRIX getViewMatrix() const;
        DirectX::XMVECTOR getPosition() const;

        void move(const DirectX::XMVECTOR& dv = { 0.0f, 0.0f, 0.0f });
        void moveNormal(float dn);
        void moveTangent(float dt);

        void rotateHorisontal(float angle);
        void rotateVertical(float angle);

        void positionClip(const WorldBorders& b);

    private:
        DirectX::XMVECTOR getTangentVector();

        DirectX::XMVECTOR _pos;
        DirectX::XMVECTOR _dir;
        DirectX::XMVECTOR _up;

        float _vertical_angle = 0.0f;
    };
}
