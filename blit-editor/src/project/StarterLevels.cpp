// the levels new projects and File > New Level start with

#include "geometry/Brush.h"
#include "project/Create.h"

Node BrushNode(const std::string& name, Vec3 lo, Vec3 hi, const std::string& texture, BrushRole role)
{
    Node node;
    Element brush = MakeElement(ElementType::Brush);
    Vec3 middle = (lo + hi) * 0.5f;

    node.name = name;
    node.position = middle;
    brush.role = role;
    brush.brushes.push_back(MakeBox(lo - middle, hi - middle, texture));
    node.elements.push_back(brush);
    return node;
}

Node PointNode(const std::string& name, Vec3 at, ElementType type)
{
    Node node;
    node.name = name;
    node.position = at;
    node.elements.push_back(MakeElement(type));
    return node;
}

Level Numbered(Level level)
{
    level.nextId = 1;

    for (Node& node : level.nodes)
    {
        Reassign(node, level);
    }

    return level;
}

Level StarterLevel3D()
{
    Level level;
    Node room;
    room.name = "Room";
    room.children.push_back(BrushNode("Floor", { -8, -0.25f, -8 }, { 8, 0, 8 }, "checker", BrushRole::Solid));

    Node start = PointNode("PlayerStart", { 0, 0.75f, 4 }, ElementType::Spawn);
    start.rotation = { 0, 90, 0 };

    Node light = PointNode("Light", { 0, 3, 0 }, ElementType::Light);
    light.elements[0].range = 10;

    level.world = { { "message", "a new level" } };
    level.nodes = { room, start, light };
    return Numbered(level);
}

Level StarterLevel2D()
{
    Level level;
    level.is2D = true;
    level.texelsPerMetre = 1.0f;

    // pixels, Y down: the ground along the bottom of a 1280x720 screen and two platforms
    Node platforms;
    platforms.name = "Platforms";
    platforms.children.push_back(BrushNode("Ground", { 0, 640, -16 }, { 1280, 720, 16 }, "", BrushRole::Solid));
    platforms.children.push_back(BrushNode("LeftLedge", { 160, 460, -16 }, { 480, 492, 16 }, "", BrushRole::Solid));
    platforms.children.push_back(BrushNode("RightLedge", { 760, 320, -16 }, { 1080, 352, 16 }, "", BrushRole::Solid));

    Node goal = BrushNode("Goal", { 1000, 240, -16 }, { 1064, 320, 16 }, "", BrushRole::Trigger);
    Element message = MakeElement(ElementType::Properties);
    message.values = { { "message", "You reached the goal!" } };
    goal.elements.push_back(message);

    Node start = PointNode("PlayerStart", { 96, 600, 0 }, ElementType::Spawn);

    Node sign = PointNode("Sign", { 320, 420, 0 }, ElementType::Sprite);
    sign.elements[0].texture = "checker";

    level.world = { { "message", "a new 2D level" } };
    level.nodes = { platforms, goal, start, sign };
    return Numbered(level);
}
