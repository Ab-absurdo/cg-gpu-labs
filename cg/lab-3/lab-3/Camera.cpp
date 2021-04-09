#include "Camera.h"

#include <algorithm>

using namespace DirectX;

namespace rendering {
    Camera::Camera(const XMVECTOR& pos, const XMVECTOR& dir)
        : _pos(pos) {
        _dir = XMVector3Normalize(dir);
        _up = { 0.0f, 1.0f, 0.0f };
    }

    XMMATRIX Camera::getViewMatrix() const {
        return XMMatrixLookAtLH(_pos, _pos + _dir, _up);
    }

    XMVECTOR Camera::getPosition() const {
        return _pos;
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
        _dir = XMVector3Rotate(_dir, XMQuaternionRotationAxis(-getTangentVector(), angle));
    }

    void Camera::positionClip(const WorldBorders& b) {
        _pos = XMVectorClamp(_pos, b._min, b._max);
    }

    XMVECTOR Camera::getTangentVector() {
        return XMVector3Normalize(XMVector3Cross(_dir, _up));
    }
}
