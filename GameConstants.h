#pragma once

namespace game {

    inline constexpr float kPlayerSphereScale = 0.6f;
    inline constexpr float kGridPlaneSize = 20.0f;
    inline constexpr float kLevelHalf = kGridPlaneSize * 0.5f;
    inline constexpr float kPropSpawnInset = 1.75f;

    inline constexpr int kRandomMeshProps = 40;
    inline constexpr float kMinPropSpawnRadius = 3.5f;
    inline constexpr float kMinPropSpawnDistSq = kMinPropSpawnRadius * kMinPropSpawnRadius;

    inline constexpr float kOrbitMouseSensitivity = 0.0045f;
    inline constexpr float kOrbitPitchLimit = 0.48f;

    inline constexpr float kMeshScaleMin = 0.075f;
    inline constexpr float kMeshScaleMax = 0.26f;

    inline constexpr float kMoaiAnchorScaleA = 0.12f;
    inline constexpr float kMoaiAnchorScaleB = 0.11f;

    inline constexpr float kStoneAnchorScaleA = 0.006f;
    inline constexpr float kStoneAnchorScaleB = 0.0055f;
    inline constexpr float kStoneRandomScaleFactor = 0.024f;
}
