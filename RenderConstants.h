#pragma once

#include <DirectXMath.h>

namespace megaEngine {

    struct CBPerObject
    {
        DirectX::XMMATRIX world;
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX proj;
        DirectX::XMMATRIX lightViewProj;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT4 lightDirAmbient;
        DirectX::XMFLOAT4 shadowParams; // x = enable, y = bias, z = texelSize, w = unused
    };

}
