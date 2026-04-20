#pragma once

#include <string>

namespace game::paths {

    inline const std::wstring kMoaiObj =
        LR"(./models/moai-low-poly-game-ready/source/Moai/Moai.obj)";
    inline const std::wstring kMoaiBaseColor =
        LR"(./models/moai-low-poly-game-ready/textures/DefaultMaterial_BaseColor.png)";

    inline const std::wstring kStoneObj = LR"(./models/stone-prop/Stone_low.obj)";
    inline const std::wstring kStoneTex = LR"(./models/stone-prop/stone_albedo.jpg)";

    inline const wchar_t kBgMusicFile[] = L"Est_Est_Est_-_Svyatki.mp3";
}
