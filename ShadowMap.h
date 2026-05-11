#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>

namespace megaEngine {

    class ShadowMap
    {
    public:
        ShadowMap() = default;
        ~ShadowMap();

        bool Initialize(ID3D11Device* device, UINT size = 2048);
        void Shutdown();

        // lightDirToLight = unit vector pointing from the scene toward the light
        // sceneCenter / sceneHalfExtent describe the AABB the shadow should cover
        void SetDirectionalLight(const DirectX::XMFLOAT3& lightDirToLight,
            const DirectX::XMFLOAT3& sceneCenter,
            float sceneHalfExtent);

        void BeginRender(ID3D11DeviceContext* context);
        void EndRender(ID3D11DeviceContext* context);

        DirectX::XMMATRIX GetView() const { return view_; }
        DirectX::XMMATRIX GetProj() const { return proj_; }
        DirectX::XMMATRIX GetViewProj() const { return DirectX::XMMatrixMultiply(view_, proj_); }

        ID3D11ShaderResourceView* GetSrv() const { return srv_.Get(); }
        ID3D11SamplerState* GetSampler() const { return sampler_.Get(); }
        UINT GetSize() const { return size_; }

    private:
        UINT size_ = 2048;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> rs_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> dss_;
        D3D11_VIEWPORT vp_ = {};
        DirectX::XMMATRIX view_ = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX proj_ = DirectX::XMMatrixIdentity();
    };

}
