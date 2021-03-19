#pragma once

#include <directxmath.h>

#include "WorldBorders.h"

class Camera
{
public:
    Camera();
    Camera(DirectX::XMVECTOR pos, DirectX::XMVECTOR dir);

    DirectX::XMMATRIX getViewMatrix();
    void move(float dx = 0.0f, float dy = 0.0f, float dz = 0.0f);
    void moveNormal(float dn);
    void moveTangent(float dt);
    void rotateHorisontal(float angle);
    void rotateVertical(float angle);
    void positionClip(WorldBorders b);

private:
    DirectX::XMVECTOR getTangentVector();

    DirectX::XMVECTOR _pos, _dir, _up;
    float _vertical_angle = 0.0f;
};
