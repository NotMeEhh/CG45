#pragma once

#include <DirectXMath.h>

namespace game {

    void InitOrbitFromOffset(const DirectX::XMFLOAT3& offset,
        float& outYaw, float& outPitch, float& outDistance);

    DirectX::XMFLOAT3 OrbitCameraPosition(const DirectX::XMFLOAT3& target,
        float yaw, float pitch, float distance);

    void ApplyOrbitMouseDelta(float& yaw, float& pitch, int mouseDx, int mouseDy);
}
