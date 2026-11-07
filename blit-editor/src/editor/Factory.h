#pragma once

// the kinds of node the Add menu makes, each a node with the elements it needs, built around its own origin

#include <string>
#include <vector>

#include "level/Level.h"

enum class NodeKind
{
    Group, Box, Wedge, Cylinder, Room, Trigger, Collision, Door,
    Light, Spawn, Marker, Camera, Sound, Model, ScriptObject,
    Sprite, Solid2D, Count
};

struct NodeKindInfo
{
    NodeKind kind;
    const char* title;
    const char* section;    // "Geometry", "Gameplay", "Points", "2D"
    bool only2D;
    bool only3D;
};

const std::vector<NodeKindInfo>& NodeKinds();
const NodeKindInfo& KindInfo(NodeKind kind);

Element BrushElement(BrushRole role, std::vector<Brush> brushes);
// size is the grid step: boxes are a few steps big. texture is what brushes get, "" for none
Node MakeNodeOfKind(NodeKind kind, bool is2D, float gridStep, const std::string& texture);
