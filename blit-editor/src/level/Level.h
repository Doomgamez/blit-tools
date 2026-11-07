#pragma once

// a level: a tree of nodes, each with a transform and a list of elements. it's saved as levels/<name>.blvl and
// compiled by the engine's blitlevel into <name>.lvl.glb. what each element becomes is in
// homeinvasionsdl/tools/blitlevel/elements.h, keep the two in step
//
// 3D levels are in engine space (Y up, metres), 2D levels in pixels with Y down and everything at z 0

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "core/Math.h"

// one side of a convex brush, the plane in the node's space
struct Face
{
    Vec3 normal{ 0, 1, 0 };     // outward, unit length
    float distance = 0.0f;      // Dot(normal, p) == distance on the plane
    std::string texture;
    float offset[2] = { 0, 0 }; // texels
    float scale[2] = { 1, 1 };  // 1 is the level's texel density
    float rotation = 0.0f;      // degrees
};

struct Brush
{
    std::vector<Face> faces;
};

enum class ElementType
{
    Brush, Light, Model, Spawn, Marker, Camera, Sound, Sprite, Mover, Script, Properties, Count
};

// how a brush draws and collides
enum class BrushRole { Solid, Detail, Collision, Trigger, Count };

using Pairs = std::vector<std::pair<std::string, std::string>>;

// one fat struct rather than a class per element, every element only reads its own fields
struct Element
{
    ElementType type = ElementType::Properties;

    // Brush
    BrushRole role = BrushRole::Solid;
    std::vector<Brush> brushes;

    // Light (color), Model and Sprite (tint), 0-255
    Vec3 color{ 255, 255, 255 };
    float range = 6.0f;
    float intensity = 1.0f;

    // Model, Sound: a file under the project's assets/
    std::string file;

    // Camera
    float fov = 60.0f;

    // Sound
    float volume = 1.0f;
    float radius = 10.0f;
    bool loop = true;

    // Sprite
    std::string texture;
    Vec2 size{ 32, 32 };
    int layer = 0;
    bool tiled = false;

    // Mover
    bool door = true;           // func_door, else func_move
    float speed = 90.0f;

    // Script: the BlitScript class (EXTENDS LevelObject) the engine makes for it
    std::string className;

    // Properties
    Pairs values;
};

struct Node
{
    int id = 0;                 // unique in the level, kept in the file so undo and selection survive a reload
    std::string name = "Node";
    std::string className;      // the entity's classname when the elements shouldn't decide it
    Vec3 position{ 0, 0, 0 };
    Vec3 rotation{ 0, 0, 0 };   // pitch yaw roll degrees, Y * X * Z
    Vec3 scale{ 1, 1, 1 };
    bool visible = true;        // in the editor only
    bool locked = false;        // can't be picked in the viewport
    bool exported = true;       // false leaves it and its children out of the compiled level
    std::vector<Element> elements;
    std::vector<Node> children;
};

struct Level
{
    bool is2D = false;
    float texelsPerMetre = 32.0f;   // 3D: texels of a texture per metre at scale 1. 2D levels use 1
    Pairs world;                    // worldspawn's properties: music, script, message...
    std::vector<Node> nodes;
    int nextId = 1;
};

// display names, file names (what's in the .blvl) and lookups
const char* ElementTitle(ElementType type);
const char* ElementKey(ElementType type);
ElementType ElementFromKey(const std::string& key);
const char* RoleTitle(BrushRole role);
const char* RoleKey(BrushRole role);
BrushRole RoleFromKey(const std::string& key);

// what the compiler gives an element as its class, "" when it adds to another class
const char* ElementClass(ElementType type);
// the classname blitlevel gives the node (Level.cpp mirrors nodes.c), "" for a plain group or geometry
std::string NodeClass(const Node& node, bool is2D);

Element MakeElement(ElementType type);
bool HasElement(const Node& node, ElementType type);
Element* FindElement(Node& node, ElementType type);

// node lookups by id, nullptr when it isn't there
Node* FindNode(std::vector<Node>& nodes, int id);
const Node* FindNode(const std::vector<Node>& nodes, int id);
// the list the node is in, and where in it
std::vector<Node>* FindSiblings(std::vector<Node>& nodes, int id, size_t& index);
// the parent's id, 0 for a top-level node, -1 for one that isn't there
int FindParentId(const std::vector<Node>& nodes, int id, int parent = 0);
bool IsDescendant(const std::vector<Node>& nodes, int ancestor, int id);

// the node's matrix in the level, through its parents
Mat4 NodeLocal(const Node& node);
Mat4 NodeWorld(const std::vector<Node>& nodes, int id);

// new ids for a node and everything under it (pasted or duplicated nodes)
void Reassign(Node& node, Level& level);
// a name no other node has: "Box", "Box 2", "Box 3"
std::string UniqueName(const Level& level, const std::string& wanted);
// calls visit on every node, parents first
void ForEachNode(std::vector<Node>& nodes, const std::function<void(Node&)>& visit);
void ForEachNode(const std::vector<Node>& nodes, const std::function<void(const Node&)>& visit);
