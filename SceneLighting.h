#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

namespace megaEngine {

    struct SceneLighting
    {
        DirectX::XMMATRIX lightViewProj = DirectX::XMMatrixIdentity();
        DirectX::XMFLOAT4 lightDirAmbient = { 0.45f, 1.0f, 0.35f, 0.28f };
        DirectX::XMFLOAT4 shadowParams = { 0.0f, 0.005f, 1.0f / 2048.0f, 0.0f };
        ID3D11ShaderResourceView* shadowSrv = nullptr;
        ID3D11SamplerState* shadowSampler = nullptr;
    };

}
