#include "editor/Factory.h"

#include <algorithm>

#include "geometry/Brush.h"

const std::vector<NodeKindInfo> kKinds = {
    { NodeKind::Group, "Group (empty node)", "Nodes", false, false },
    { NodeKind::Box, "Box brush", "Geometry", false, true },
    { NodeKind::Wedge, "Wedge (ramp)", "Geometry", false, true },
    { NodeKind::Cylinder, "Cylinder", "Geometry", false, true },
    { NodeKind::Room, "Room (hollow box)", "Geometry", false, true },
    { NodeKind::Solid2D, "Solid rectangle", "Geometry", true, false },
    { NodeKind::Trigger, "Trigger volume", "Gameplay", false, false },
    { NodeKind::Collision, "Invisible wall", "Gameplay", false, false },
    { NodeKind::Door, "Door (mover + script)", "Gameplay", false, true },
    { NodeKind::ScriptObject, "Script object", "Gameplay", false, false },
    { NodeKind::Spawn, "Player start", "Gameplay", false, false },
    { NodeKind::Light, "Point light", "Points", false, false },
    { NodeKind::Marker, "Marker", "Points", false, false },
    { NodeKind::Camera, "Camera", "Points", false, false },
    { NodeKind::Sound, "Sound", "Points", false, false },
    { NodeKind::Model, "Model (glTF prop)", "Points", false, true },
    { NodeKind::Sprite, "Sprite", "Points", true, false },
};

const std::vector<NodeKindInfo>& NodeKinds()
{
    return kKinds;
}

const NodeKindInfo& KindInfo(NodeKind kind)
{
    for (const NodeKindInfo& info : kKinds)
    {
        if (info.kind == kind) return info;
    }

    return kKinds.front();
}

Element BrushElement(BrushRole role, std::vector<Brush> brushes)
{
    Element element = MakeElement(ElementType::Brush);
    element.role = role;
    element.brushes = std::move(brushes);
    return element;
}

Node MakeNodeOfKind(NodeKind kind, bool is2D, float gridStep, const std::string& texture)
{
    Node node;
    // 3D: a metre-ish box on the grid, 2D: a few grid cells, flat in z
    float s = std::max(gridStep, is2D ? 8.0f : 0.125f) * (is2D ? 4.0f : 4.0f);
    Vec3 half = is2D ? Vec3(s, s * 0.5f, 16) : Vec3(s * 0.5f, s * 0.5f, s * 0.5f);
    std::string tex = is2D ? "" : texture;

    node.name = KindInfo(kind).title;
    node.name = node.name.substr(0, node.name.find(' '));

    switch (kind)
    {
    case NodeKind::Group:
        node.name = "Group";
        break;
    case NodeKind::Box:
    case NodeKind::Solid2D:
        node.name = is2D ? "Solid" : "Box";
        node.elements.push_back(BrushElement(BrushRole::Solid, { MakeBox(-half, half, tex) }));
        break;
    case NodeKind::Wedge:
        node.elements.push_back(BrushElement(BrushRole::Solid, { MakeWedge(-half, half, tex) }));
        break;
    case NodeKind::Cylinder:
        node.elements.push_back(BrushElement(BrushRole::Solid, { MakeCylinder(-half, half, 12, tex) }));
        break;
    case NodeKind::Room:
    {
        Vec3 room = half * 4.0f;
        node.elements.push_back(BrushElement(BrushRole::Solid, MakeHollow(-room, room, std::max(gridStep, 0.125f), tex)));
        break;
    }
    case NodeKind::Trigger:
        node.elements.push_back(BrushElement(BrushRole::Trigger, { MakeBox(-half, half, "trigger") }));
        node.elements.push_back(MakeElement(ElementType::Script));
        break;
    case NodeKind::Collision:
        node.name = "InvisibleWall";
        node.elements.push_back(BrushElement(BrushRole::Collision, { MakeBox(-half, half, "clip") }));
        break;
    case NodeKind::Door:
    {
        Vec3 door = { half.x, half.y * 2.0f, std::max(gridStep, 0.125f) * 0.5f };
        node.elements.push_back(BrushElement(BrushRole::Solid, { MakeBox(-door, door, tex) }));
        node.elements.push_back(MakeElement(ElementType::Mover));
        Element script = MakeElement(ElementType::Script);
        script.className = "Door";
        node.elements.push_back(script);
        break;
    }
    case NodeKind::ScriptObject:
        node.name = "Object";
        node.elements.push_back(MakeElement(ElementType::Script));
        break;
    case NodeKind::Spawn: node.name = "PlayerStart"; node.elements.push_back(MakeElement(ElementType::Spawn)); break;
    case NodeKind::Light:
    {
        node.name = "Light";
        Element light = MakeElement(ElementType::Light);
        light.range = is2D ? 200.0f : 6.0f;
        node.elements.push_back(light);
        break;
    }
    case NodeKind::Marker: node.elements.push_back(MakeElement(ElementType::Marker)); break;
    case NodeKind::Camera: node.elements.push_back(MakeElement(ElementType::Camera)); break;
    case NodeKind::Sound: node.elements.push_back(MakeElement(ElementType::Sound)); break;
    case NodeKind::Model: node.elements.push_back(MakeElement(ElementType::Model)); break;
    case NodeKind::Sprite:
    {
        Element sprite = MakeElement(ElementType::Sprite);
        sprite.texture = texture;
        sprite.size = { s, s };
        node.elements.push_back(sprite);
        break;
    }
    default:
        break;
    }

    return node;
}
