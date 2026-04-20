#pragma once

#include "GameComponent.h"
#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

namespace megaEngine {

    class MeshComponent : public GameComponent {
    public:
        enum class Type { Box, Sphere, FbxFile, ObjFile };

        MeshComponent(Type type, const std::wstring& meshPath = L"",
            const std::wstring& diffuseTexturePath = L"");
        ~MeshComponent();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, HWND hwnd) override;
        void Update(float deltaTime) override;
        void Render(ID3D11DeviceContext* context, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj) override;
        void Shutdown() override;

        void SetOrbitParams(MeshComponent* parent, float radius, float orbitSpeed, const DirectX::XMFLOAT4& color);
        DirectX::XMFLOAT3 GetWorldPosition() const;
        void SetPosition(const DirectX::XMFLOAT3& pos) { position_ = pos; }
        float GetOrbitRadius() const { return orbitRadius_; }
        MeshComponent* GetParent() const { return parent_; }

        void SetScale(float s) { scale_ = s; }
        float GetScale() const { return scale_; }

        DirectX::XMMATRIX GetWorldMatrix() const;
        float GetCollisionRadius() const;
        bool IsKatamariPickable() const;
        bool IsStuckToBall() const { return stickParent_ != nullptr; }
        void AttachToKatamariBall(MeshComponent* ball);

        void ApplySphereRollXZ(float distanceMovedWorld, float dirX, float dirZ);

    private:
        struct Vertex { DirectX::XMFLOAT4 pos; DirectX::XMFLOAT4 color; DirectX::XMFLOAT4 normal; DirectX::XMFLOAT2 uv; };

        bool CompileShader(const wchar_t* filename, const char* entryPoint,
            const char* target, ID3DBlob** blob, D3D_SHADER_MACRO* macros = nullptr);

        void CreateBox(float size);
        void CreateSphere(float radius, int slices, int stacks);
        bool LoadFbxAsciiFile();
        bool LoadObjFile();
        bool EnsureDiffuseResources(ID3D11Device* device, ID3D11DeviceContext* context);
        void CreateGpuMesh(const std::vector<Vertex>& verts, const std::vector<UINT>& indices);

        Type type_;
        std::wstring meshPath_;
        std::wstring diffuseTexturePath_;
        DirectX::XMMATRIX importBasis_ = DirectX::XMMatrixIdentity();
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> diffuseSrv_;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> diffuseSampler_;

        ID3D11DeviceContext* context_ = nullptr;
        UINT stride_ = sizeof(Vertex);
        UINT offset_ = 0;
        UINT indexCount_ = 0;

        DirectX::XMFLOAT3 position_ = { 0,0,0 };
        float orbitRadius_ = 0.0f;
        float orbitSpeed_ = 0.0f;
        DirectX::XMFLOAT3 selfAngles_ = {0,0,0};
        float orbitAngle_ = 0.0f;
        MeshComponent* parent_ = nullptr;
        DirectX::XMFLOAT4 color_ = {1,1,1,1};

        float scale_ = 1.0f;

        MeshComponent* stickParent_ = nullptr;
        DirectX::XMFLOAT4X4 stickMeshToBall_ = {};
        float pickupBoundsRadius_ = 0.35f;

        DirectX::XMFLOAT4 sphereRollQuat_ = { 0.f, 0.f, 0.f, 1.f };
    };
}
