#include "Camera.h"

#include <algorithm>

using namespace DirectX;

namespace rendering {
    Camera::Camera() {
        _pos = { 0.0f, 0.0f, 0.0f };
        _dir = { 0.0f, 0.0f, 1.0f };
        _up = { 0.0f, 1.0f, 0.0f };
    }

    Camera::Camera(const DirectX::XMVECTOR& pos, const DirectX::XMVECTOR& dir)
        : _pos(pos) {
        _dir = XMVector3Normalize(dir);
        _up = { 0.0f, 1.0f, 0.0f };
    }

    XMMATRIX Camera::getViewMatrix() {
        return XMMatrixLookAtLH(_pos, _pos + _dir, _up);
    }

    void Camera::move(const XMVECTOR& dv) {
        _pos += dv;
    }

    void Camera::moveNormal(float dn) {
        _pos += dn * _dir;
    }

    void Camera::moveTangent(float dt) {
        _pos += dt * getTangentVector();
    }

    void Camera::rotateHorisontal(float angle) {
        _dir = XMVector3Rotate(_dir, XMQuaternionRotationAxis(_up, angle));
    }

    void Camera::rotateVertical(float angle) {
        angle = std::clamp(angle, -XM_PIDIV2 - _vertical_angle, XM_PIDIV2 - _vertical_angle);
        _vertical_angle += angle;
        _dir = DirectX::XMVector3Rotate(_dir, DirectX::XMQuaternionRotationAxis(-getTangentVector(), angle));
    }

    void Camera::positionClip(const WorldBorders& b) {
        _pos = DirectX::XMVectorClamp(_pos, b._min, b._max);
    }

    DirectX::XMVECTOR Camera::getTangentVector() {
        return DirectX::XMVector3Normalize(DirectX::XMVector3Cross(_dir, _up));
    }
}
