#pragma once

#include <DirectXMath.h>
#include "InputDevice.h"

namespace megaEngine {
    enum class CameraMode { FPS, ORBIT };

    class Camera {
    public:
        Camera();

        void SetPerspective(float fovY, float aspect, float zn, float zf);
        void SetAspect(float aspect);
        void SetOrthoHeight(float height);
        void Update(float deltaTime, const InputDevice& input);

        DirectX::XMMATRIX GetViewMatrix() const;
        DirectX::XMMATRIX GetProjMatrix() const;

        void ToggleMode();
        void CyclePerspective();

        // control helpers for following target
        void SetPosition(const DirectX::XMFLOAT3& pos);
        DirectX::XMFLOAT3 GetPosition() const;
        void LookAt(const DirectX::XMFLOAT3& target);
        // get forward/right vectors in XZ plane (normalized)
        DirectX::XMFLOAT3 GetForwardXZ() const;
        DirectX::XMFLOAT3 GetRightXZ() const;

        bool IsOrthographic() const { return orthographic_; }
        CameraMode GetMode() const { return mode_; }

    private:
        CameraMode mode_;

        // FPS
        DirectX::XMFLOAT3 position_;
        float yaw_, pitch_;

        // Orbit
        DirectX::XMFLOAT3 orbitTarget_;
        float orbitDistance_;
        float orbitAngle_;

        // Projection
        float fovY_;
        float aspect_;
        float zn_, zf_;
        bool orthographic_;
        float orthoHeight_;
        float orthoWidth_;

        void UpdateFPS(float deltaTime, const InputDevice& input);
        void UpdateOrbit(float deltaTime, const InputDevice& input);
    };
}
