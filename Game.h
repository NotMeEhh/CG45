#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include "GameComponent.h"
#include "DisplayWin32.h"
#include "InputDevice.h"
#include "Camera.h"
#include "MeshComponent.h"
#include "GridRenderer.h"
#include "GameAudio.h"

using megaEngine::GameComponent;
using megaEngine::DisplayWin32;
using megaEngine::InputDevice;
using megaEngine::Camera;

namespace game {

    class Game
    {
    public:
        Game();
        ~Game();

        bool Initialize(HINSTANCE hInstance);
        void Run();
        void Shutdown();

    private:
        bool InitializeDirect3D();
        void RenderFrame();
        void UpdateFPS(float deltaTime);
        bool ResizeResources(int width, int height);
        void ProcessKatamariPickups();

        DisplayWin32 display_;
        InputDevice input_;

        Microsoft::WRL::ComPtr<ID3D11Device> device_;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
        Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilTexture_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState_;

        std::vector<std::unique_ptr<GameComponent>> components_;
        std::unique_ptr<megaEngine::GridRenderer> gridRenderer_;

        int screenWidth_ = 800;
        int screenHeight_ = 800;

        float totalTime_ = 0.0f;
        unsigned int frameCount_ = 0;

        Camera camera_;

        megaEngine::MeshComponent* player_ = nullptr;
        DirectX::XMFLOAT3 cameraOffset_ = { 0.0f, 2.65f, -6.2f };
        float orbitCameraYaw_ = 0.0f;
        float orbitCameraPitch_ = 0.0f;
        float orbitCameraDistance_ = 1.0f;
        float playerSpeed_ = 2.5f;
        unsigned katamariPickups_ = 0;
        BackgroundMusic backgroundMusic_;
    };
}