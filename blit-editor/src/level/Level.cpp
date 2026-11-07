#include "level/Level.h"

struct ElementInfo
{
    const char* title;
    const char* key;
    const char* className;
};

const ElementInfo kElements[(int)ElementType::Count] = {
    { "Brush", "brush", "" },
    { "Point light", "light", "light_point" },
    { "Model", "model", "prop_model" },
    { "Player start", "spawn", "info_player_start" },
    { "Marker", "marker", "info_target" },
    { "Camera", "camera", "info_camera" },
    { "Sound", "sound", "ambient_sound" },
    { "Sprite", "sprite", "sprite_2d" },
    { "Text", "text", "label" },
    { "Mover", "mover", "" },
    { "Script (BlitScript object)", "script", "" },
    { "Properties", "properties", "" },
};

const char* const kRoleTitles[(int)BrushRole::Count] = { "Solid", "Detail (no collision)", "Collision (invisible)", "Trigger" };
const char* const kRoleKeys[(int)BrushRole::Count] = { "solid", "detail", "collision", "trigger" };

const char* ElementTitle(ElementType type) { return kElements[(int)type].title; }
const char* ElementKey(ElementType type) { return kElements[(int)type].key; }
const char* ElementClass(ElementType type) { return kElements[(int)type].className; }
const char* RoleTitle(BrushRole role) { return kRoleTitles[(int)role]; }
const char* RoleKey(BrushRole role) { return kRoleKeys[(int)role]; }

ElementType ElementFromKey(const std::string& key)
{
    for (int i = 0; i < (int)ElementType::Count; i++)
    {
        if (key == kElements[i].key)
        {
            return (ElementType)i;
        }
    }

    return ElementType::Count;
}

BrushRole RoleFromKey(const std::string& key)
{
    for (int i = 0; i < (int)BrushRole::Count; i++)
    {
        if (key == kRoleKeys[i])
        {
            return (BrushRole)i;
        }
    }

    return BrushRole::Solid;
}

std::string NodeClass(const Node& node, bool is2D)
{
    bool script = false, properties = false;

    if (!node.className.empty())
    {
        return node.className;
    }

    for (const Element& e : node.elements)
    {
        if (e.type == ElementType::Mover) return e.door ? "func_door" : "func_move";

        if (e.type == ElementType::Brush)
        {
            if (e.role == BrushRole::Trigger) return "trigger_script";
            if (e.role == BrushRole::Collision) return "func_collision";
            if (e.role == BrushRole::Detail) return "func_detail";
            if (is2D) return "solid_2d";
        }
    }

    for (const Element& e : node.elements)
    {
        if (ElementClass(e.type)[0]) return ElementClass(e.type);
        script |= e.type == ElementType::Script;
        properties |= e.type == ElementType::Properties;
    }

    return script ? "object_script" : properties ? "info_null" : "";
}

Element MakeElement(ElementType type)
{
    Element element;
    element.type = type;

    if (type == ElementType::Light)
    {
        element.color = { 255, 230, 200 };
    }

    return element;
}

bool HasElement(const Node& node, ElementType type)
{
    for (const Element& e : node.elements)
    {
        if (e.type == type) return true;
    }

    return false;
}

Element* FindElement(Node& node, ElementType type)
{
    for (Element& e : node.elements)
    {
        if (e.type == type) return &e;
    }

    return nullptr;
}

Node* FindNode(std::vector<Node>& nodes, int id)
{
    for (Node& node : nodes)
    {
        if (node.id == id) return &node;
        if (Node* found = FindNode(node.children, id)) return found;
    }

    return nullptr;
}

const Node* FindNode(const std::vector<Node>& nodes, int id)
{
    return FindNode(const_cast<std::vector<Node>&>(nodes), id);
}

std::vector<Node>* FindSiblings(std::vector<Node>& nodes, int id, size_t& index)
{
    for (size_t i = 0; i < nodes.size(); i++)
    {
        if (nodes[i].id == id)
        {
            index = i;
            return &nodes;
        }

        if (std::vector<Node>* found = FindSiblings(nodes[i].children, id, index)) return found;
    }

    return nullptr;
}

int FindParentId(const std::vector<Node>& nodes, int id, int parent)
{
    for (const Node& node : nodes)
    {
        if (node.id == id) return parent;

        int found = FindParentId(node.children, id, node.id);
        if (found >= 0) return found;
    }

    return -1;
}

bool IsDescendant(const std::vector<Node>& nodes, int ancestor, int id)
{
    const Node* top = FindNode(nodes, ancestor);
    return top && FindNode(top->children, id) != nullptr;
}

Mat4 NodeLocal(const Node& node)
{
    return Mat4Compose(node.position, node.rotation, node.scale);
}

Mat4 NodeWorld(const std::vector<Node>& nodes, int id)
{
    Mat4 world;

    for (int at = id; at > 0; at = FindParentId(nodes, at))
    {
        const Node* node = FindNode(nodes, at);

        if (!node) break;
        world = NodeLocal(*node) * world;
    }

    return world;
}

void Reassign(Node& node, Level& level)
{
    node.id = level.nextId++;

    for (Node& child : node.children)
    {
        Reassign(child, level);
    }
}

std::string UniqueName(const Level& level, const std::string& wanted)
{
    std::string base = wanted.empty() ? "Node" : wanted;

    for (int suffix = 1;; suffix++)
    {
        std::string name = suffix == 1 ? base : base + " " + std::to_string(suffix);
        bool taken = false;

        ForEachNode(level.nodes, [&](const Node& node) { taken |= node.name == name; });

        if (!taken) return name;
    }
}

void ForEachNode(std::vector<Node>& nodes, const std::function<void(Node&)>& visit)
{
    for (Node& node : nodes)
    {
        visit(node);
        ForEachNode(node.children, visit);
    }
}

void ForEachNode(const std::vector<Node>& nodes, const std::function<void(const Node&)>& visit)
{
    for (const Node& node : nodes)
    {
        visit(node);
        ForEachNode(node.children, visit);
    }
}
