#pragma once

#include <windows.h>
#include <memory>
#include <random>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace megaEngine {
    class MeshComponent;
    class GameComponent;
}

namespace game {

    struct SceneSpawnContext {
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        HWND hwnd = nullptr;
        std::vector<std::unique_ptr<megaEngine::GameComponent>>* components = nullptr;
        megaEngine::MeshComponent** playerOut = nullptr;
    };

    struct SceneAssetState {
        bool moai = false;
        bool stone = false;
    };

    SceneAssetState ProbeSceneAssets();

    bool PopulateSceneMeshes(const SceneSpawnContext& ctx, std::mt19937& rng);
}
