#include "ShadowMap.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace megaEngine {

    ShadowMap::~ShadowMap() { Shutdown(); }

    bool ShadowMap::Initialize(ID3D11Device* device, UINT size)
    {
        if (!device) return false;
        size_ = size;

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = size_;
        td.Height = size_;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&td, nullptr, &tex_))) return false;

        D3D11_DEPTH_STENCIL_VIEW_DESC dvd = {};
        dvd.Format = DXGI_FORMAT_D32_FLOAT;
        dvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        if (FAILED(device->CreateDepthStencilView(tex_.Get(), &dvd, &dsv_))) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
        svd.Format = DXGI_FORMAT_R32_FLOAT;
        svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        svd.Texture2D.MipLevels = 1;
        if (FAILED(device->CreateShaderResourceView(tex_.Get(), &svd, &srv_))) return false;

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.BorderColor[0] = 1.0f;
        sd.BorderColor[1] = 1.0f;
        sd.BorderColor[2] = 1.0f;
        sd.BorderColor[3] = 1.0f;
        sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sd.MinLOD = 0.0f;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sd, &sampler_))) return false;

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        rd.DepthBias = 100;
        rd.SlopeScaledDepthBias = 1.5f;
        rd.DepthBiasClamp = 0.0f;
        if (FAILED(device->CreateRasterizerState(&rd, &rs_))) return false;

        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc = D3D11_COMPARISON_LESS;
        dsd.StencilEnable = FALSE;
        if (FAILED(device->CreateDepthStencilState(&dsd, &dss_))) return false;

        vp_.TopLeftX = 0.0f;
        vp_.TopLeftY = 0.0f;
        vp_.Width = static_cast<float>(size_);
        vp_.Height = static_cast<float>(size_);
        vp_.MinDepth = 0.0f;
        vp_.MaxDepth = 1.0f;

        return true;
    }

    void ShadowMap::SetDirectionalLight(const XMFLOAT3& lightDirToLight,
        const XMFLOAT3& sceneCenter, float sceneHalfExtent)
    {
        XMVECTOR L = XMVector3Normalize(XMLoadFloat3(&lightDirToLight));
        const float halfDiag = sceneHalfExtent * 1.732f;
        const float distA = halfDiag * 2.0f;
        const float distB = sceneHalfExtent * 2.5f;
        const float dist = distA > distB ? distA : distB;

        XMVECTOR centerV = XMLoadFloat3(&sceneCenter);
        XMVECTOR eye = XMVectorAdd(centerV, XMVectorScale(L, dist));

        XMVECTOR up = (fabsf(XMVectorGetY(L)) > 0.95f)
            ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
            : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        view_ = XMMatrixLookAtLH(eye, centerV, up);

        const float orthoSize = sceneHalfExtent * 2.4f;
        const float nearZ = 0.1f;
        const float farZ = dist + sceneHalfExtent * 2.0f + 5.0f;
        proj_ = XMMatrixOrthographicLH(orthoSize, orthoSize, nearZ, farZ);
    }

    void ShadowMap::BeginRender(ID3D11DeviceContext* context)
    {
        ID3D11RenderTargetView* nullRtv[1] = { nullptr };
        context->OMSetRenderTargets(1, nullRtv, dsv_.Get());
        context->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        context->RSSetViewports(1, &vp_);
        context->RSSetState(rs_.Get());
        context->OMSetDepthStencilState(dss_.Get(), 0);
        context->PSSetShader(nullptr, nullptr, 0);
    }

    void ShadowMap::EndRender(ID3D11DeviceContext* context)
    {
        ID3D11RenderTargetView* nullRtv[1] = { nullptr };
        context->OMSetRenderTargets(1, nullRtv, nullptr);
    }

    void ShadowMap::Shutdown()
    {
        tex_.Reset();
        dsv_.Reset();
        srv_.Reset();
        sampler_.Reset();
        rs_.Reset();
        dss_.Reset();
    }

}
