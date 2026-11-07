#pragma once

// a level's lighting, kept in its world properties the way the game reads them (ApplyLevelLighting in
// homeinvasionsdl/src/Api/ApiLevel.c): shadowColor, sun, sunIntensity, sunColor, sunDirection
//
// everything is lit by the shadow color, the sun and the point lights added up, never brighter than the surface:
// a white shadow color is no shadow at all, black is pitch black where no light reaches. in 2D the sun lights
// everything evenly

#include "level/Level.h"

struct LevelLighting
{
    Vec3 shadow{ 115, 115, 115 };           // 0-255, the engine's default
    bool sun = true;
    float sunIntensity = 0.55f;
    Vec3 sunColor{ 255, 255, 255 };         // 0-255
    Vec3 sunDirection{ -0.4f, -0.8f, -0.45f };  // the way it shines
};

// what the level asks for, the engine's defaults for what it doesn't say
LevelLighting ReadLevelLighting(const Level& level);
// every setting into the world properties
void WriteLevelLighting(Level& level, const LevelLighting& lighting);

// the pieces: a world property by key, "" when there's none, and setting one
std::string WorldProperty(const Level& level, const std::string& key);
void SetWorldProperty(Level& level, const std::string& key, const std::string& value);
// "a b c" into three numbers, false when it isn't three
bool ReadThree(const std::string& text, Vec3& out);
