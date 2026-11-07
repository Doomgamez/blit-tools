#pragma once

// a level to and from its .blvl JSON, the format homeinvasionsdl/tools/blitlevel reads (docs/LevelSystem.md)

#include <string>

#include "core/Json.h"
#include "level/Level.h"

constexpr int kLevelFormat = 1;

Json LevelToJson(const Level& level);
// false, and why in error, for JSON that isn't a level
bool LevelFromJson(const Json& json, Level& level, std::string& error);

bool SaveLevel(const std::string& path, const Level& level);
bool LoadLevel(const std::string& path, Level& level, std::string& error);

// the pieces, also used for copy and paste
Json NodeToJson(const Node& node);
Node NodeFromJson(const Json& json, Level& level);
Json ElementToJson(const Element& element);
Element ElementFromJson(const Json& json);
Json BrushToJson(const Brush& brush);
Brush BrushFromJson(const Json& json);
Json PairsToJson(const Pairs& pairs);
Pairs PairsFromJson(const Json* json);
Json VecToJson(Vec3 v);
Vec3 VecFromJson(const Json& json, const std::string& key, Vec3 fallback);

// a sprite's "fit": scaled, tiled, repeat
const char* SpriteFitKey(SpriteFit fit);
SpriteFit SpriteFitFromKey(const std::string& key);
