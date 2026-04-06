#pragma once

#include "GameComponent.h"
#include "MeshComponent.h"
#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <DirectXMath.h>

namespace megaEngine {
    class GridRenderer
    {
    public:
        GridRenderer();
        ~GridRenderer();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, float size = 20.0f, float spacing = 1.0f);
        void Render(ID3D11DeviceContext* context, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);
        void Shutdown();

    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> dssOff_;

        UINT vertexCount_ = 0;
    };
}
