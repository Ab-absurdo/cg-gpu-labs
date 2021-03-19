#include "Camera.h"

#include <windows.h>

using namespace DirectX;

Camera::Camera()
{
    _pos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    _dir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    _up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
}

Camera::Camera(XMVECTOR pos, XMVECTOR dir) : _pos(pos)
{
    _dir = XMVector4Normalize(dir);
    _up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
}

XMMATRIX Camera::getViewMatrix()
{
    XMVECTOR focus = XMVectorAdd(_pos, _dir);
    return XMMatrixLookAtLH(_pos, focus, _up);
}

void Camera::move(float dx, float dy, float dz)
{
    _pos = XMVectorSet(XMVectorGetX(_pos) + dx, XMVectorGetY(_pos) + dy, XMVectorGetZ(_pos) + dz, 0.0f);
}

void Camera::moveNormal(float dn)
{
    _pos = XMVectorAdd(_pos, XMVectorScale(_dir, dn));
}

void Camera::moveTangent(float dt)
{
    XMVECTOR tangent = getTangentVector();
    _pos = XMVectorAdd(_pos, XMVectorScale(tangent, dt));
}

void Camera::rotateHorisontal(float angle)
{
    XMVECTOR rotation_quaternion = XMQuaternionRotationAxis(_up, angle);
    _dir = XMVector3Rotate(_dir, rotation_quaternion);
}

void Camera::rotateVertical(float angle)
{
    XMVECTOR tangent = getTangentVector();
    angle = min(angle, XM_PIDIV2 - _vertical_angle);
    angle = max(angle, -XM_PIDIV2 - _vertical_angle);
    _vertical_angle += angle;
    XMVECTOR rotation_quaternion = XMQuaternionRotationAxis(-tangent, angle);
    _dir = XMVector3Rotate(_dir, rotation_quaternion);
}

void Camera::positionClip(WorldBorders b)
{
    if (XMVectorGetX(_pos) > b.x_max)
        _pos = XMVectorSetX(_pos, b.x_max);
    if (XMVectorGetX(_pos) < b.x_min)
        _pos = XMVectorSetX(_pos, b.x_min);
    if (XMVectorGetY(_pos) > b.y_max)
        _pos = XMVectorSetY(_pos, b.y_max);
    if (XMVectorGetY(_pos) < b.y_min)
        _pos = XMVectorSetY(_pos, b.y_min);
    if (XMVectorGetZ(_pos) > b.z_max)
        _pos = XMVectorSetZ(_pos, b.z_max);
    if (XMVectorGetZ(_pos) < b.z_min)
        _pos = XMVectorSetZ(_pos, b.z_min);
}

XMVECTOR Camera::getTangentVector()
{
    return XMVector4Normalize(XMVector3Cross(_dir, _up));
}
