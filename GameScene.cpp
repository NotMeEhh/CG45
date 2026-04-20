#include "GameScene.h"
#include "GameConstants.h"
#include "GamePaths.h"
#include "MeshComponent.h"
#include "GameComponent.h"

#include <filesystem>
#include <iostream>

using megaEngine::GameComponent;
using megaEngine::MeshComponent;

namespace game {

    SceneAssetState ProbeSceneAssets()
    {
        SceneAssetState s;
        const std::filesystem::path moaiObj(paths::kMoaiObj);
        s.moai = std::filesystem::exists(moaiObj) && std::filesystem::is_regular_file(moaiObj);
        if (!s.moai)
            std::wcerr << L"Moai OBJ not found at " << paths::kMoaiObj << L" (props will be skipped).\n";

        std::error_code ec;
        const std::filesystem::path stoneObj(paths::kStoneObj);
        const std::filesystem::path stoneTex(paths::kStoneTex);
        s.stone = std::filesystem::is_regular_file(stoneObj, ec) &&
            std::filesystem::is_regular_file(stoneTex, ec);
        if (!s.stone)
            std::wcerr << L"Stone prop (OBJ + albedo) not found (optional).\n";

        return s;
    }

    namespace {

        bool TryAddObjMesh(const SceneSpawnContext& ctx, const std::wstring& obj, const std::wstring& tex,
            const DirectX::XMFLOAT3& pos, float scale, const DirectX::XMFLOAT4& color, const wchar_t* errTag)
        {
            auto mesh = std::make_unique<MeshComponent>(MeshComponent::Type::ObjFile, obj, tex);
            mesh->SetOrbitParams(nullptr, 0.0f, 0.0f, color);
            mesh->SetPosition(pos);
            mesh->SetScale(scale);
            if (mesh->Initialize(ctx.device, ctx.context, ctx.hwnd)) {
                ctx.components->push_back(std::move(mesh));
                return true;
            }
            std::wcerr << errTag << std::endl;
            return false;
        }
    }

    bool PopulateSceneMeshes(const SceneSpawnContext& ctx, std::mt19937& rng)
    {
        if (!ctx.device || !ctx.context || !ctx.hwnd || !ctx.components || !ctx.playerOut)
            return false;

        const SceneAssetState assets = ProbeSceneAssets();

        auto player = std::make_unique<MeshComponent>(MeshComponent::Type::Sphere);
        if (!player->Initialize(ctx.device, ctx.context, ctx.hwnd))
            return false;
        player->SetOrbitParams(nullptr, 0.0f, 0.0f, DirectX::XMFLOAT4(0.2f, 0.7f, 0.9f, 1.0f));
        player->SetScale(kPlayerSphereScale);
        player->SetPosition(DirectX::XMFLOAT3(0.0f, 0.5f * kPlayerSphereScale, 0.0f));
        ctx.components->push_back(std::move(player));
        *ctx.playerOut = static_cast<MeshComponent*>(ctx.components->back().get());

        const DirectX::XMFLOAT4 white(1.0f, 1.0f, 1.0f, 1.0f);

        if (assets.moai) {
            TryAddObjMesh(ctx, paths::kMoaiObj, paths::kMoaiBaseColor,
                DirectX::XMFLOAT3(2.8f, 0.0f, 0.0f), kMoaiAnchorScaleA, white,
                L"Could not load Moai OBJ + texture.");
            TryAddObjMesh(ctx, paths::kMoaiObj, paths::kMoaiBaseColor,
                DirectX::XMFLOAT3(-3.2f, 0.0f, 1.0f), kMoaiAnchorScaleB, white,
                L"Could not load Moai OBJ + texture.");
        }

        if (assets.stone) {
            TryAddObjMesh(ctx, paths::kStoneObj, paths::kStoneTex,
                DirectX::XMFLOAT3(0.0f, 0.0f, 4.2f), kStoneAnchorScaleA, white,
                L"Could not load Stone_low OBJ + albedo.");
            TryAddObjMesh(ctx, paths::kStoneObj, paths::kStoneTex,
                DirectX::XMFLOAT3(-4.0f, 0.0f, -2.5f), kStoneAnchorScaleB, white,
                L"Could not load Stone_low OBJ + albedo.");
        }

        if (!assets.moai && !assets.stone)
            return true;

        std::uniform_real_distribution<float> rndMeshScale(kMeshScaleMin, kMeshScaleMax);
        const float spawnHalf = kLevelHalf - kPropSpawnInset;
        std::uniform_real_distribution<float> rndPos(-spawnHalf, spawnHalf);

        for (int i = 0; i < kRandomMeshProps; ++i) {
            float x = 0.f, z = 0.f;
            for (int a = 0; a < 24; ++a) {
                x = rndPos(rng);
                z = rndPos(rng);
                if (x * x + z * z >= kMinPropSpawnDistSq) break;
            }
            const float sc = rndMeshScale(rng);
            const bool useStone = assets.stone && (!assets.moai || (i % 4) == 0);
            const std::wstring& meshPath = useStone ? paths::kStoneObj : paths::kMoaiObj;
            const std::wstring& texPath = useStone ? paths::kStoneTex : paths::kMoaiBaseColor;
            if (!assets.moai && !useStone) continue;
            if (!assets.stone && useStone) continue;

            const float propScale = useStone ? sc * kStoneRandomScaleFactor : sc;
            TryAddObjMesh(ctx, meshPath, texPath, DirectX::XMFLOAT3(x, 0.0f, z), propScale, white,
                L"Random prop mesh failed to load (skipped).");
        }

        return true;
    }
}
