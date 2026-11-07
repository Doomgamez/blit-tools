#include "level/LevelLighting.h"

#include <cstdio>

std::string WorldProperty(const Level& level, const std::string& key)
{
    for (const auto& pair : level.world)
    {
        if (pair.first == key) return pair.second;
    }

    return "";
}

void SetWorldProperty(Level& level, const std::string& key, const std::string& value)
{
    for (auto& pair : level.world)
    {
        if (pair.first == key)
        {
            pair.second = value;
            return;
        }
    }

    level.world.push_back({ key, value });
}

bool ReadThree(const std::string& text, Vec3& out)
{
    float a, b, c;

    if (std::sscanf(text.c_str(), "%f %f %f", &a, &b, &c) != 3) return false;

    out = { a, b, c };
    return true;
}

LevelLighting ReadLevelLighting(const Level& level)
{
    LevelLighting lighting;
    std::string sun = WorldProperty(level, "sun");
    float intensity;

    ReadThree(WorldProperty(level, "shadowColor"), lighting.shadow);
    ReadThree(WorldProperty(level, "sunColor"), lighting.sunColor);
    ReadThree(WorldProperty(level, "sunDirection"), lighting.sunDirection);
    lighting.sun = !(sun == "false" || sun == "0" || sun == "no" || sun == "off");

    if (std::sscanf(WorldProperty(level, "sunIntensity").c_str(), "%f", &intensity) == 1)
    {
        lighting.sunIntensity = intensity;
    }

    return lighting;
}

void WriteLevelLighting(Level& level, const LevelLighting& lighting)
{
    char text[96];

    std::snprintf(text, sizeof(text), "%.0f %.0f %.0f", lighting.shadow.x, lighting.shadow.y, lighting.shadow.z);
    SetWorldProperty(level, "shadowColor", text);
    SetWorldProperty(level, "sun", lighting.sun ? "true" : "false");
    std::snprintf(text, sizeof(text), "%g", lighting.sunIntensity);
    SetWorldProperty(level, "sunIntensity", text);
    std::snprintf(text, sizeof(text), "%.0f %.0f %.0f", lighting.sunColor.x, lighting.sunColor.y, lighting.sunColor.z);
    SetWorldProperty(level, "sunColor", text);
    std::snprintf(text, sizeof(text), "%g %g %g", lighting.sunDirection.x, lighting.sunDirection.y, lighting.sunDirection.z);
    SetWorldProperty(level, "sunDirection", text);
}
