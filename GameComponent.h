#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

namespace megaEngine {
    class GameComponent
    {
    public:
        virtual ~GameComponent() = default;

        virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, HWND hwnd) { return true; }

        virtual void Update(float deltaTime) {}

        virtual void Render(ID3D11DeviceContext* context, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj) {}

        virtual void Shutdown() {}
    };
}