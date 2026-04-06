#include "Camera.h"
#include <DirectXMath.h>
#include <cmath>
#include <Windows.h>

using namespace DirectX;
using megaEngine::Camera;

Camera::Camera()
    : mode_(CameraMode::ORBIT), position_{0.0f, 0.0f, -5.0f}, yaw_(0.0f), pitch_(0.0f),
      orbitTarget_{0.0f, 0.0f, 0.0f}, orbitDistance_(10.0f), orbitAngle_(0.0f),
      fovY_(XM_PIDIV4), aspect_(1.0f), zn_(0.1f), zf_(1000.0f), orthographic_(false), orthoHeight_(10.0f), orthoWidth_(10.0f)
{
}

void Camera::SetPerspective(float fovY, float aspect, float zn, float zf)
{
    fovY_ = fovY; aspect_ = aspect; zn_ = zn; zf_ = zf;
    orthoWidth_ = orthoHeight_ * aspect_;
}

void Camera::SetAspect(float aspect)
{
    aspect_ = aspect;
    orthoWidth_ = orthoHeight_ * aspect_;
}

void Camera::SetOrthoHeight(float height)
{
    orthoHeight_ = height;
    orthoWidth_ = orthoHeight_ * aspect_;
}

void Camera::ToggleMode() { mode_ = (mode_ == CameraMode::FPS) ? CameraMode::ORBIT : CameraMode::FPS; }

void Camera::CyclePerspective()
{
    // Cycle among a few fov presets
    if (fabsf(fovY_ - XM_PIDIV4) < 0.001f) fovY_ = XM_PIDIV2; // 90 deg
    else if (fabsf(fovY_ - XM_PIDIV2) < 0.001f) fovY_ = XM_PI / 6.0f; // 30 deg
    else fovY_ = XM_PIDIV4;
}

void Camera::Update(float deltaTime, const InputDevice& input)
{
    if (mode_ == CameraMode::FPS) UpdateFPS(deltaTime, input);
    else UpdateOrbit(deltaTime, input);
}

void Camera::UpdateFPS(float deltaTime, const InputDevice& input)
{
    const float moveSpeed = 5.0f;
    const float rotSpeed = 1.5f;

    // compute camera-local forward and right from yaw/pitch
    XMVECTOR forward = XMVectorSet(cosf(pitch_) * sinf(yaw_), sinf(pitch_), cosf(pitch_) * cosf(yaw_), 0.0f);
    forward = XMVector3Normalize(forward);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, up));

    XMVECTOR dir = XMVectorZero();

    auto KeyDown = [&](int vkey)->bool {
        return input.IsKeyPressed(static_cast<unsigned int>(vkey)) || (GetAsyncKeyState(vkey) & 0x8000) != 0;
    };

    if (KeyDown('W')) dir += forward;
    if (KeyDown('S')) dir -= forward;
    if (KeyDown('A')) dir -= right;
    if (KeyDown('D')) dir += right;

    XMFLOAT3 d;
    XMStoreFloat3(&d, dir);
    position_.x += d.x * moveSpeed * deltaTime;
    position_.y += d.y * moveSpeed * deltaTime;
    position_.z += d.z * moveSpeed * deltaTime;

    if (KeyDown(VK_LEFT)) yaw_ -= rotSpeed * deltaTime;
    if (KeyDown(VK_RIGHT)) yaw_ += rotSpeed * deltaTime;
    if (KeyDown(VK_UP)) pitch_ -= rotSpeed * deltaTime;
    if (KeyDown(VK_DOWN)) pitch_ += rotSpeed * deltaTime;
}

void Camera::UpdateOrbit(float deltaTime, const InputDevice& input)
{
    const float orbitSpeed = 0.5f;
    auto KeyDown = [&](int vkey)->bool {
        return input.IsKeyPressed(static_cast<unsigned int>(vkey)) || (GetAsyncKeyState(vkey) & 0x8000) != 0;
    };
    if (KeyDown('Q')) orbitAngle_ -= orbitSpeed * deltaTime;
    if (KeyDown('E')) orbitAngle_ += orbitSpeed * deltaTime;
    if (KeyDown('R')) orbitDistance_ -= 5.0f * deltaTime;
    if (KeyDown('F')) orbitDistance_ += 5.0f * deltaTime;


    const float panSpeed = 5.0f;
    XMFLOAT3 pan = { 0.0f, 0.0f, 0.0f };
    XMVECTOR camForward = XMVectorSet(sinf(orbitAngle_), 0.0f, cosf(orbitAngle_), 0.0f);
    camForward = XMVector3Normalize(camForward);
    XMVECTOR camRight = XMVector3Normalize(XMVector3Cross(camForward, XMVectorSet(0,1,0,0)));

    if (KeyDown('W')) {
        XMFLOAT3 f; XMStoreFloat3(&f, camForward); pan.x += f.x; pan.z += f.z;
    }
    if (KeyDown('S')) {
        XMFLOAT3 f; XMStoreFloat3(&f, camForward); pan.x -= f.x; pan.z -= f.z;
    }
    if (KeyDown('A')) {
        XMFLOAT3 r; XMStoreFloat3(&r, camRight); pan.x -= r.x; pan.z -= r.z;
    }
    if (KeyDown('D')) {
        XMFLOAT3 r; XMStoreFloat3(&r, camRight); pan.x += r.x; pan.z += r.z;
    }

    orbitTarget_.x += pan.x * panSpeed * deltaTime;
    orbitTarget_.y += pan.y * panSpeed * deltaTime;
    orbitTarget_.z += pan.z * panSpeed * deltaTime;
}

DirectX::XMMATRIX Camera::GetViewMatrix() const
{
    if (mode_ == CameraMode::FPS) {
        XMVECTOR pos = XMLoadFloat3(&position_);
        XMVECTOR lookDir = XMVectorSet(cosf(pitch_) * sinf(yaw_), sinf(pitch_), cosf(pitch_) * cosf(yaw_), 0);
        XMVECTOR target = pos + lookDir;
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        return XMMatrixLookAtLH(pos, target, up);
    }
    else {
        float x = orbitTarget_.x + orbitDistance_ * sinf(orbitAngle_);
        float z = orbitTarget_.z + orbitDistance_ * cosf(orbitAngle_);
        XMVECTOR pos = XMVectorSet(x, orbitTarget_.y + orbitDistance_ * 0.2f, z, 0);
        XMVECTOR target = XMLoadFloat3(&orbitTarget_);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        return XMMatrixLookAtLH(pos, target, up);
    }
}

DirectX::XMMATRIX Camera::GetProjMatrix() const
{
    if (orthographic_) {
        return XMMatrixOrthographicLH(orthoWidth_, orthoHeight_, zn_, zf_);
    }
    return XMMatrixPerspectiveFovLH(fovY_, aspect_, zn_, zf_);
}

void Camera::SetPosition(const XMFLOAT3& pos)
{
    position_ = pos;
}

DirectX::XMFLOAT3 Camera::GetPosition() const
{
    return position_;
}

void Camera::LookAt(const XMFLOAT3& target)
{
    // compute yaw/pitch so that camera looks at target from position_
    XMVECTOR p = XMLoadFloat3(&position_);
    XMVECTOR t = XMLoadFloat3(&target);
    XMVECTOR dir = XMVectorSubtract(t, p);
    dir = XMVector3Normalize(dir);
    float dx = XMVectorGetX(dir);
    float dy = XMVectorGetY(dir);
    float dz = XMVectorGetZ(dir);
    // yaw rotates around Y, pitch around X
    yaw_ = atan2f(dx, dz);
    pitch_ = asinf(dy);
}

DirectX::XMFLOAT3 Camera::GetForwardXZ() const
{
    float fx = cosf(pitch_) * sinf(yaw_);
    float fz = cosf(pitch_) * cosf(yaw_);
    XMFLOAT3 f = { fx, 0.0f, fz };
    XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&f));
    XMFLOAT3 out; XMStoreFloat3(&out, v);
    return out;
}

DirectX::XMFLOAT3 Camera::GetRightXZ() const
{
    // right = cross(up, forward) to get the world-right direction
    XMFLOAT3 f = GetForwardXZ();
    XMVECTOR fv = XMLoadFloat3(&f);
    XMVECTOR up = XMVectorSet(0,1,0,0);
    XMVECTOR rv = XMVector3Normalize(XMVector3Cross(up, fv));
    XMFLOAT3 out; XMStoreFloat3(&out, rv);
    out.y = 0.0f;
    // normalize again in XZ
    XMVECTOR nv = XMVector3Normalize(XMLoadFloat3(&out));
    XMStoreFloat3(&out, nv);
    return out;
}


