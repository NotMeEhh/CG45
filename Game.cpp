#include "Game.h"
#include "MeshComponent.h"
#include "GridRenderer.h"

#include <dxgi.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <windows.h>
#include <WinUser.h>
#include <wrl.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <random>
#include <vector>
#include <cctype>
#include <algorithm>


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")


using megaEngine::MeshComponent;
namespace game {

    namespace {
        constexpr float kPlayerSphereScale = 0.3f;
        constexpr float kGridPlaneSize = 20.0f;
        constexpr float kLevelHalf = kGridPlaneSize * 0.5f;
        constexpr float kPropSpawnInset = 1.75f;
    }

    Game::Game() = default;
    Game::~Game() { Shutdown(); }

    bool Game::Initialize(HINSTANCE hInstance)
    {
        if (!display_.Initialize(L"Katamari (roll into props)", screenWidth_, screenHeight_, hInstance, &input_))
            return false;

        if (!InitializeDirect3D()) return false;

        camera_.SetPerspective(DirectX::XM_PIDIV4, static_cast<float>(screenWidth_) / screenHeight_, 0.1f, 1000.0f);

        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_real_distribution<float> rndMeshScale(0.032f, 0.09f);
        const float spawnHalf = kLevelHalf - kPropSpawnInset;
        std::uniform_real_distribution<float> rndPos(-spawnHalf, spawnHalf);
        std::uniform_real_distribution<float> rndColor(0.25f, 1.0f);

        std::vector<std::wstring> modelsFbx;
        {
            const std::filesystem::path modelsDir("./models");
            std::error_code ec;
            if (std::filesystem::exists(modelsDir, ec) && std::filesystem::is_directory(modelsDir, ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(modelsDir, ec)) {
                    if (ec || !entry.is_regular_file()) continue;
                    std::string ext = entry.path().extension().string();
                    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (ext != ".fbx") continue;
                    std::filesystem::path rel = modelsDir / entry.path().filename();
                    modelsFbx.push_back(rel.wstring());
                }
            }
        }

        auto player = std::make_unique<megaEngine::MeshComponent>(megaEngine::MeshComponent::Type::Sphere);
        if (!player->Initialize(device_.Get(), context_.Get(), display_.GetHwnd())) return false;
        player->SetOrbitParams(nullptr, 0.0f, 0.0f, DirectX::XMFLOAT4(0.2f, 0.7f, 0.9f, 1.0f));
        player->SetScale(kPlayerSphereScale);
        player->SetPosition(DirectX::XMFLOAT3(0.0f, 0.5f * kPlayerSphereScale, 0.0f));
        components_.push_back(std::move(player));
        player_ = static_cast<megaEngine::MeshComponent*>(components_.back().get());

        auto tryLoadOptionalFbx = [&](const wchar_t* path, const DirectX::XMFLOAT3& pos, float scale,
            const DirectX::XMFLOAT4& color) {
            const std::filesystem::path fbxPath(path);
            if (!std::filesystem::exists(fbxPath)) {
                std::wcerr << L"Mesh file not found (skipped): " << path << std::endl;
                return;
            }
            if (!std::filesystem::is_regular_file(fbxPath)) {
                std::wcerr << L"Mesh path is not a file (skipped): " << path << std::endl;
                return;
            }
            auto mesh = std::make_unique<megaEngine::MeshComponent>(
                megaEngine::MeshComponent::Type::FbxFile, path);
            mesh->SetOrbitParams(nullptr, 0.0f, 0.0f, color);
            mesh->SetPosition(pos);
            mesh->SetScale(scale);
            if (mesh->Initialize(device_.Get(), context_.Get(), display_.GetHwnd()))
                components_.push_back(std::move(mesh));
            else {
                std::wcerr << L"Could not load mesh (need ASCII FBX): " << path << std::endl;
            }
        };

        tryLoadOptionalFbx(L"./models/kettle.fbx",
            DirectX::XMFLOAT3(2.8f, 0.0f, 0.0f), 0.045f,
            DirectX::XMFLOAT4(0.92f, 0.5f, 0.18f, 1.0f));

        tryLoadOptionalFbx(L"./models/kolba.fbx",
            DirectX::XMFLOAT3(-3.2f, 0.0f, 1.0f), 0.04f,
            DirectX::XMFLOAT4(0.35f, 0.55f, 0.9f, 1.0f));

        constexpr int kRandomMeshProps = 40;
        constexpr float kMinSpawnDistSq = 3.5f * 3.5f;
        if (modelsFbx.empty()) {
            std::wcerr << L"No .fbx files found in ./models/ (random props skipped).\n";
        } else {
            std::uniform_int_distribution<size_t> pickModel(0, modelsFbx.size() - 1);
            for (int i = 0; i < kRandomMeshProps; ++i) {
                float x = 0.f, z = 0.f;
                for (int a = 0; a < 24; ++a) {
                    x = rndPos(rng);
                    z = rndPos(rng);
                    if (x * x + z * z >= kMinSpawnDistSq) break;
                }
                const std::wstring& path = modelsFbx[pickModel(rng)];
                const float sc = rndMeshScale(rng);
                auto mesh = std::make_unique<megaEngine::MeshComponent>(
                    megaEngine::MeshComponent::Type::FbxFile, path);
                mesh->SetOrbitParams(nullptr, 0.0f, 0.0f,
                    DirectX::XMFLOAT4(rndColor(rng), rndColor(rng), rndColor(rng), 1.0f));
                mesh->SetScale(sc);
                mesh->SetPosition(DirectX::XMFLOAT3(x, 0.0f, z));
                if (mesh->Initialize(device_.Get(), context_.Get(), display_.GetHwnd()))
                    components_.push_back(std::move(mesh));
                else
                    std::wcerr << L"Random mesh failed to load (skipped): " << path.c_str() << std::endl;
            }
        }

        gridRenderer_ = std::make_unique<megaEngine::GridRenderer>();
        if (!gridRenderer_->Initialize(device_.Get(), context_.Get(), kGridPlaneSize, 1.0f)) return false;

        if (camera_.GetMode() != megaEngine::CameraMode::FPS) camera_.ToggleMode();
        DirectX::XMFLOAT3 ppos = player_->GetWorldPosition();
        DirectX::XMFLOAT3 camPos = { ppos.x + cameraOffset_.x, ppos.y + cameraOffset_.y, ppos.z + cameraOffset_.z };
        camera_.SetPosition(camPos);
        camera_.LookAt(ppos);

        return true;
    }

    bool Game::InitializeDirect3D()
    {
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;

        DXGI_SWAP_CHAIN_DESC swapDesc = {};
        swapDesc.BufferCount = 2;
        swapDesc.BufferDesc.Width = screenWidth_;
        swapDesc.BufferDesc.Height = screenHeight_;
        swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.BufferDesc.RefreshRate = { 60, 1 };
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.OutputWindow = display_.GetHwnd();
        swapDesc.Windowed = TRUE;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapDesc.SampleDesc.Count = 1;

        HRESULT res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_DEBUG, &featureLevel, 1,
            D3D11_SDK_VERSION, &swapDesc, &swapChain_, &device_, nullptr, &context_);

        if (FAILED(res)) { std::cerr << "D3D11CreateDeviceAndSwapChain failed: " << res << std::endl; return false; }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
        if (FAILED(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView_))) return false;


        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = screenWidth_;
        depthDesc.Height = screenHeight_;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        if (FAILED(device_->CreateTexture2D(&depthDesc, nullptr, &depthStencilTexture_))) return false;
        if (FAILED(device_->CreateDepthStencilView(depthStencilTexture_.Get(), nullptr, &depthStencilView_))) return false;

        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
        dsDesc.StencilEnable = FALSE;

        if (FAILED(device_->CreateDepthStencilState(&dsDesc, &depthStencilState_))) return false;

        context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), depthStencilView_.Get());
        context_->OMSetDepthStencilState(depthStencilState_.Get(), 0);

        return true;
    }

    bool Game::ResizeResources(int width, int height)
    {
        if (width <= 0 || height <= 0) return false;
        if (!swapChain_) return false;

        screenWidth_ = width;
        screenHeight_ = height;

        renderTargetView_.Reset();
        depthStencilView_.Reset();
        depthStencilTexture_.Reset();

        HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return false;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
        if (FAILED(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView_))) return false;

        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        if (FAILED(device_->CreateTexture2D(&depthDesc, nullptr, &depthStencilTexture_))) return false;
        if (FAILED(device_->CreateDepthStencilView(depthStencilTexture_.Get(), nullptr, &depthStencilView_))) return false;

        context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), depthStencilView_.Get());

        camera_.SetAspect(static_cast<float>(width) / static_cast<float>(height));

        return true;
    }

    void Game::Run()
    {
        auto prevTime = std::chrono::steady_clock::now();

        bool prevC = false;
        bool prevP = false;

        while (display_.IsRunning())
        {
            if (!display_.ProcessMessages(&input_)) break;

            auto curTime = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(curTime - prevTime).count();
            prevTime = curTime;

            RECT rc; GetClientRect(display_.GetHwnd(), &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            if (w != screenWidth_ || h != screenHeight_) {
                ResizeResources(w, h);
            }

            bool cPressed = input_.IsKeyPressed('C');
            if (cPressed && !prevC) camera_.ToggleMode();
            prevC = cPressed;

            bool pPressed = input_.IsKeyPressed('P');
            if (pPressed && !prevP) camera_.CyclePerspective();
            prevP = pPressed;

            camera_.Update(deltaTime, input_);

            if (player_) {
                auto clampPlayerXZToLevel = [&] {
                    DirectX::XMFLOAT3 p = player_->GetWorldPosition();
                    const float R = 0.5f * player_->GetScale();
                    float lim = kLevelHalf - R;
                    if (lim < 0.f) lim = 0.f;
                    p.x = std::clamp(p.x, -lim, lim);
                    p.z = std::clamp(p.z, -lim, lim);
                    p.y = 0.5f * player_->GetScale();
                    player_->SetPosition(p);
                };

                DirectX::XMFLOAT3 dir = {0,0,0};
                DirectX::XMFLOAT3 f = camera_.GetForwardXZ();
                DirectX::XMFLOAT3 r = camera_.GetRightXZ();
                if (input_.IsKeyPressed('W')) { dir.x += f.x; dir.z += f.z; }
                if (input_.IsKeyPressed('S')) { dir.x -= f.x; dir.z -= f.z; }
                if (input_.IsKeyPressed('A')) { dir.x -= r.x; dir.z -= r.z; }
                if (input_.IsKeyPressed('D')) { dir.x += r.x; dir.z += r.z; }
                float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                DirectX::XMFLOAT3 pos = player_->GetWorldPosition();
                if (len > 1e-6f) {
                    dir.x /= len; dir.y /= len; dir.z /= len;
                    float move = playerSpeed_ * deltaTime;
                    pos.x += dir.x * move;
                    pos.z += dir.z * move;
                    player_->ApplySphereRollXZ(move, dir.x, dir.z);
                }
                pos.y = 0.5f * player_->GetScale();
                player_->SetPosition(pos);
                clampPlayerXZToLevel();

                ProcessKatamariPickups();
                clampPlayerXZToLevel();

                DirectX::XMFLOAT3 ppos = player_->GetWorldPosition();
                DirectX::XMFLOAT3 camPos = { ppos.x + cameraOffset_.x, ppos.y + cameraOffset_.y, ppos.z + cameraOffset_.z };
                camera_.SetPosition(camPos);
                camera_.LookAt(ppos);
            }

            for (auto& comp : components_) comp->Update(deltaTime);
            UpdateFPS(deltaTime);

            RenderFrame();
        }
    }

    void Game::RenderFrame()
    {
        float clearColor[] = { 0.05f, 0.06f, 0.09f, 1.0f };
        context_->ClearState();
        context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
        if (depthStencilView_) context_->ClearDepthStencilView(depthStencilView_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        D3D11_VIEWPORT vp = { 0, 0, static_cast<float>(screenWidth_),
                             static_cast<float>(screenHeight_), 0.0f, 1.0f };
        context_->RSSetViewports(1, &vp);
        context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), depthStencilView_.Get());

        auto view = camera_.GetViewMatrix();
        auto proj = camera_.GetProjMatrix();

        if (gridRenderer_)
            gridRenderer_->Render(context_.Get(), view, proj);

        for (auto& comp : components_) comp->Render(context_.Get(), view, proj);

        swapChain_->Present(1, 0);
    }

    void Game::ProcessKatamariPickups()
    {
        if (!player_) return;
        DirectX::XMFLOAT3 pc = player_->GetWorldPosition();
        const float pr = player_->GetCollisionRadius();

        for (auto& up : components_) {
            auto* m = dynamic_cast<megaEngine::MeshComponent*>(up.get());
            if (!m || m == player_ || !m->IsKatamariPickable()) continue;

            DirectX::XMFLOAT3 oc = m->GetWorldPosition();
            float dx = oc.x - pc.x, dy = oc.y - pc.y, dz = oc.z - pc.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            float reach = pr + m->GetCollisionRadius();
            if (distSq < reach * reach) {
                float ns = player_->GetScale() * 1.045f;
                if (ns > 1.15f) ns = 1.15f;
                player_->SetScale(ns);
                m->AttachToKatamariBall(player_);
                ++katamariPickups_;
            }
        }
    }

    void Game::UpdateFPS(float deltaTime)
    {
        totalTime_ += deltaTime;
        frameCount_++;

        if (totalTime_ >= 1.0f) {
            float fps = frameCount_ / totalTime_;
            wchar_t title[160];
            swprintf_s(title, L"Katamari — picked: %u | FPS: %.1f", katamariPickups_, fps);
            SetWindowText(display_.GetHwnd(), title);

            totalTime_ = 0.0f;
            frameCount_ = 0;
        }
    }

    void Game::Shutdown()
    {
        if (gridRenderer_) gridRenderer_->Shutdown();
        gridRenderer_.reset();

        for (auto& comp : components_) comp->Shutdown();
        components_.clear();

        renderTargetView_.Reset();
        depthStencilView_.Reset();
        depthStencilTexture_.Reset();
        depthStencilState_.Reset();
        context_.Reset();
        device_.Reset();
        swapChain_.Reset();

        display_.Shutdown();
    }

}