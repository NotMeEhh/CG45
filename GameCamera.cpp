#include "GameCamera.h"
#include "GameConstants.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace game {

    void InitOrbitFromOffset(const XMFLOAT3& offset, float& outYaw, float& outPitch, float& outDistance)
    {
        const float ox = offset.x, oy = offset.y, oz = offset.z;
        outDistance = sqrtf(ox * ox + oy * oy + oz * oz);
        if (outDistance < 1e-4f) outDistance = 1.0f;
        outYaw = atan2f(ox, oz);
        outPitch = asinf(std::clamp(oy / outDistance, -1.0f, 1.0f));
    }

    XMFLOAT3 OrbitCameraPosition(const XMFLOAT3& target, float yaw, float pitch, float distance)
    {
        const float cosp = cosf(pitch);
        const float sinp = sinf(pitch);
        const float siny = sinf(yaw);
        const float cosy = cosf(yaw);
        return {
            target.x + distance * cosp * siny,
            target.y + distance * sinp,
            target.z + distance * cosp * cosy
        };
    }

    void ApplyOrbitMouseDelta(float& yaw, float& pitch, int mouseDx, int mouseDy)
    {
        yaw += static_cast<float>(mouseDx) * kOrbitMouseSensitivity;
        pitch -= static_cast<float>(mouseDy) * kOrbitMouseSensitivity;
        const float lim = XM_PIDIV2 * kOrbitPitchLimit;
        pitch = std::clamp(pitch, -lim, lim);
    }
}
