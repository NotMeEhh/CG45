#include "GridRenderer.h"
#include <d3dcompiler.h>
#include <iostream>

using namespace DirectX;
namespace megaEngine {

    struct CBPerObject {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX proj;
        XMFLOAT4 color;
        XMFLOAT4 lightDirAmbient;
    };

    struct VertexPosColorN { XMFLOAT4 pos; XMFLOAT4 color; XMFLOAT4 normal; XMFLOAT2 uv; };

    GridRenderer::GridRenderer() {}
    GridRenderer::~GridRenderer() { Shutdown(); }

    bool GridRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, float size, float spacing)
    {
        ID3DBlob* vsBlob = nullptr;
        if (FAILED(D3DCompileFromFile(L"./Shaders/MyVeryFirstShader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vsBlob, nullptr))) {
            std::cerr << "Failed to compile VS for GridRenderer" << std::endl;
            return false;
        }
        if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs_))) { vsBlob->Release(); return false; }

        ID3DBlob* psBlob = nullptr;
        if (FAILED(D3DCompileFromFile(L"./Shaders/MyVeryFirstShader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &psBlob, nullptr))) {
            vsBlob->Release(); std::cerr << "Failed to compile PS for GridRenderer" << std::endl; return false;
        }
        if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps_))) { vsBlob->Release(); psBlob->Release(); return false; }

        D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        if (FAILED(device->CreateInputLayout(layoutDesc, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &layout_))) { vsBlob->Release(); psBlob->Release(); return false; }
        vsBlob->Release(); psBlob->Release();


        // create a simple filled plane (quad) centered at origin using two triangles
        std::vector<VertexPosColorN> verts;
        float half = size * 0.5f;
        XMFLOAT4 color = XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f);
        XMFLOAT4 nUp = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
        const XMFLOAT2 zuv(0.f, 0.f);

        verts.push_back({ XMFLOAT4(-half, 0.0f, -half, 1.0f), color, nUp, zuv });
        verts.push_back({ XMFLOAT4(half, 0.0f, half, 1.0f), color, nUp, zuv });
        verts.push_back({ XMFLOAT4(half, 0.0f, -half, 1.0f), color, nUp, zuv });

        verts.push_back({ XMFLOAT4(-half, 0.0f, -half, 1.0f), color, nUp, zuv });
        verts.push_back({ XMFLOAT4(-half, 0.0f, half, 1.0f), color, nUp, zuv });
        verts.push_back({ XMFLOAT4(half, 0.0f, half, 1.0f), color, nUp, zuv });

        vertexCount_ = static_cast<UINT>(verts.size());
        UINT totalBytes = static_cast<UINT>(sizeof(VertexPosColorN) * verts.size());

        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.ByteWidth = totalBytes;
        D3D11_SUBRESOURCE_DATA sd = { verts.data(), 0, 0 };
        if (FAILED(device->CreateBuffer(&vbDesc, &sd, &vertexBuffer_))) return false;

        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.ByteWidth = sizeof(CBPerObject);
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &constantBuffer_))) return false;

        return true;
    }

    void GridRenderer::Render(ID3D11DeviceContext* context, const XMMATRIX& view, const XMMATRIX& proj)
    {
        if (!vertexBuffer_) return;

        context->IASetInputLayout(layout_.Get());
        UINT stride = sizeof(VertexPosColorN);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs_.Get(), nullptr, 0);
        context->PSSetShader(ps_.Get(), nullptr, 0);

        CBPerObject cb;
        cb.world = XMMatrixTranspose(XMMatrixIdentity());
        cb.view = XMMatrixTranspose(view);
        cb.proj = XMMatrixTranspose(proj);
        cb.color = XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f);
        {
            XMVECTOR L = XMVector3Normalize(XMVectorSet(0.45f, 1.0f, 0.35f, 0.0f));
            XMFLOAT3 ld;
            XMStoreFloat3(&ld, L);
            cb.lightDirAmbient = XMFLOAT4(ld.x, ld.y, ld.z, 0.28f);
        }

        context->UpdateSubresource(constantBuffer_.Get(), 0, nullptr, &cb, 0, 0);
        context->VSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());

        context->Draw(vertexCount_, 0);
    }

    void GridRenderer::Shutdown()
    {
        vertexBuffer_.Reset(); vs_.Reset(); ps_.Reset(); layout_.Reset(); constantBuffer_.Reset();
    }

}
