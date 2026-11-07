#include "level/LevelFile.h"

#include <algorithm>
#include <set>

Json VecToJson(Vec3 v)
{
    return JsonNumbers(&v.x, 3);
}

Vec3 VecFromJson(const Json& json, const std::string& key, Vec3 fallback)
{
    json.Numbers(key, &fallback.x, 3);
    return fallback;
}

Json PairsToJson(const Pairs& pairs)
{
    Json out = JsonObject();

    for (const auto& pair : pairs)
    {
        out[pair.first] = Json(pair.second);
    }

    return out;
}

Pairs PairsFromJson(const Json* json)
{
    Pairs out;

    for (size_t i = 0; json && json->IsObject() && i < json->fields.size(); i++)
    {
        out.emplace_back(json->fields[i].first, json->fields[i].second.AsPropertyText());
    }

    return out;
}

Json BrushToJson(const Brush& brush)
{
    Json out = JsonObject();
    Json& faces = out["faces"] = JsonArray();

    for (const Face& face : brush.faces)
    {
        Json f = JsonObject();
        float plane[4] = { face.normal.x, face.normal.y, face.normal.z, face.distance };

        f["plane"] = JsonNumbers(plane, 4);
        f["texture"] = Json(face.texture);

        if (face.offset[0] != 0 || face.offset[1] != 0) f["offset"] = JsonNumbers(face.offset, 2);
        if (face.scale[0] != 1 || face.scale[1] != 1) f["scale"] = JsonNumbers(face.scale, 2);
        if (face.rotation != 0) f["rotation"] = Json(face.rotation);

        faces.items.push_back(f);
    }

    return out;
}

Brush BrushFromJson(const Json& json)
{
    Brush brush;
    const Json* faces = json.Find("faces");

    for (size_t i = 0; faces && faces->IsArray() && i < faces->items.size(); i++)
    {
        const Json& f = faces->items[i];
        float plane[4] = { 0, 1, 0, 0 };
        Face face;

        f.Numbers("plane", plane, 4);
        face.normal = Normalize({ plane[0], plane[1], plane[2] });
        face.distance = plane[3] / std::fmax(Length({ plane[0], plane[1], plane[2] }), 1e-12f);
        face.texture = f.Text("texture");
        f.Numbers("offset", face.offset, 2);
        f.Numbers("scale", face.scale, 2);
        face.rotation = (float)f.Number("rotation", 0);
        brush.faces.push_back(face);
    }

    return brush;
}

Json ElementToJson(const Element& e)
{
    Json out = JsonObject();
    out["type"] = Json(ElementKey(e.type));

    switch (e.type)
    {
    case ElementType::Brush:
    {
        out["role"] = Json(RoleKey(e.role));
        Json& brushes = out["brushes"] = JsonArray();
        for (const Brush& brush : e.brushes) brushes.items.push_back(BrushToJson(brush));
        break;
    }
    case ElementType::Light:
        out["color"] = VecToJson(e.color);
        out["range"] = Json(e.range);
        out["intensity"] = Json(e.intensity);
        break;
    case ElementType::Model:
        out["file"] = Json(e.file);
        out["tint"] = VecToJson(e.color);
        break;
    case ElementType::Text:
        out["text"] = Json(e.text);
        out["size"] = Json(e.textSize);
        out["color"] = VecToJson(e.color);
        out["align"] = Json(e.align == 0 ? "left" : e.align == 2 ? "right" : "center");
        out["screen"] = Json(e.screen);
        break;
    case ElementType::Camera:
        out["fov"] = Json(e.fov);
        out["zoom"] = Json(e.zoom);
        break;
    case ElementType::Sound:
        out["file"] = Json(e.file);
        out["volume"] = Json(e.volume);
        out["radius"] = Json(e.radius);
        out["loop"] = Json(e.loop);
        break;
    case ElementType::Sprite:
        out["texture"] = Json(e.texture);
        out["fit"] = Json(SpriteFitKey(e.fit));
        out["tiles"] = JsonNumbers(&e.tiles.x, 2);
        out["flipX"] = Json(e.flipX);
        out["flipY"] = Json(e.flipY);
        out["columns"] = Json(e.columns);
        out["rows"] = Json(e.rows);
        out["frame"] = Json(e.frame);
        if (e.useRegion) out["region"] = JsonNumbers(e.region, 4);
        out["tint"] = VecToJson(e.color);
        break;
    case ElementType::Mover:
        out["kind"] = Json(e.door ? "door" : "move");
        out["speed"] = Json(e.speed);
        break;
    case ElementType::Script:
        out["class"] = Json(e.className);
        break;
    case ElementType::Properties:
        out["values"] = PairsToJson(e.values);
        break;
    default:
        break;
    }

    return out;
}

const char* SpriteFitKey(SpriteFit fit)
{
    return fit == SpriteFit::Tiled ? "tiled" : fit == SpriteFit::Repeat ? "repeat" : "scaled";
}

SpriteFit SpriteFitFromKey(const std::string& key)
{
    return key == "tiled" ? SpriteFit::Tiled : key == "repeat" ? SpriteFit::Repeat : SpriteFit::Scaled;
}

Element ElementFromJson(const Json& json)
{
    ElementType type = ElementFromKey(json.Text("type"));
    Element e = MakeElement(type == ElementType::Count ? ElementType::Properties : type);
    const Json* brushes = json.Find("brushes");

    e.role = RoleFromKey(json.Text("role", "solid"));

    for (size_t i = 0; brushes && brushes->IsArray() && i < brushes->items.size(); i++)
    {
        e.brushes.push_back(BrushFromJson(brushes->items[i]));
    }

    e.color = VecFromJson(json, type == ElementType::Light || type == ElementType::Text ? "color" : "tint", e.color);
    e.range = (float)json.Number("range", e.range);
    e.intensity = (float)json.Number("intensity", e.intensity);
    e.file = json.Text("file");
    e.fov = (float)json.Number("fov", e.fov);
    e.zoom = (float)json.Number("zoom", e.zoom);
    e.text = json.Text("text", e.text);
    e.textSize = (float)json.Number("size", e.textSize);
    e.align = json.Text("align", "center") == "left" ? 0 : json.Text("align", "center") == "right" ? 2 : 1;
    e.screen = json.Bool("screen", false);
    e.volume = (float)json.Number("volume", e.volume);
    e.radius = (float)json.Number("radius", e.radius);
    e.loop = json.Bool("loop", e.loop);
    e.texture = json.Text("texture");
    e.fit = SpriteFitFromKey(json.Text("fit", json.Bool("tiled", false) ? "tiled" : "scaled"));
    json.Numbers("tiles", &e.tiles.x, 2);
    e.flipX = json.Bool("flipX", false);
    e.flipY = json.Bool("flipY", false);
    e.columns = (int)json.Number("columns", 1);
    e.rows = (int)json.Number("rows", 1);
    e.frame = (int)json.Number("frame", 0);
    e.useRegion = json.Find("region") != nullptr;
    json.Numbers("region", e.region, 4);
    e.door = json.Text("kind", "door") == "door";
    e.speed = (float)json.Number("speed", e.speed);
    e.className = json.Text("class");
    e.values = PairsFromJson(json.Find("values"));

    // an element this editor doesn't know keeps its data as properties instead of vanishing
    if (type == ElementType::Count)
    {
        e.values.emplace_back("unknown_element", json.Text("type"));
    }

    return e;
}

Json NodeToJson(const Node& node)
{
    Json out = JsonObject();

    out["id"] = Json(node.id);
    out["name"] = Json(node.name);
    if (!node.className.empty()) out["class"] = Json(node.className);
    out["position"] = VecToJson(node.position);
    out["rotation"] = VecToJson(node.rotation);
    out["scale"] = VecToJson(node.scale);
    if (!node.visible) out["visible"] = Json(false);
    if (node.locked) out["locked"] = Json(true);
    if (!node.exported) out["export"] = Json(false);

    Json& elements = out["elements"] = JsonArray();
    for (const Element& e : node.elements) elements.items.push_back(ElementToJson(e));

    Json& children = out["children"] = JsonArray();
    for (const Node& child : node.children) children.items.push_back(NodeToJson(child));

    return out;
}

Node NodeFromJson(const Json& json, Level& level)
{
    Node node;
    const Json* elements = json.Find("elements");
    const Json* children = json.Find("children");

    // LevelFromJson gives missing and repeated ids new ones once the whole tree is read
    node.id = (int)json.Number("id", 0);
    level.nextId = std::max(level.nextId, node.id + 1);
    node.name = json.Text("name", "Node");
    node.className = json.Text("class");
    node.position = VecFromJson(json, "position", node.position);
    node.rotation = VecFromJson(json, "rotation", node.rotation);
    node.scale = VecFromJson(json, "scale", node.scale);
    node.visible = json.Bool("visible", true);
    node.locked = json.Bool("locked", false);
    node.exported = json.Bool("export", true);

    for (size_t i = 0; elements && elements->IsArray() && i < elements->items.size(); i++)
    {
        node.elements.push_back(ElementFromJson(elements->items[i]));

        // what's on top in 2D used to be a sprite's layer, it's the node's z now
        if (node.elements.back().type == ElementType::Sprite)
        {
            node.position.z += (float)elements->items[i].Number("layer", 0);
        }
    }

    for (size_t i = 0; children && children->IsArray() && i < children->items.size(); i++)
    {
        node.children.push_back(NodeFromJson(children->items[i], level));
    }

    return node;
}

Json LevelToJson(const Level& level)
{
    Json out = JsonObject();

    out["blitlevel"] = Json(kLevelFormat);
    out["mode"] = Json(level.is2D ? "2d" : "3d");
    out["texelsPerMetre"] = Json(level.texelsPerMetre);
    out["world"]["properties"] = PairsToJson(level.world);

    Json& nodes = out["nodes"] = JsonArray();
    for (const Node& node : level.nodes) nodes.items.push_back(NodeToJson(node));

    // what only the editor uses, the compiler leaves it alone
    if (!level.guidesX.empty() || !level.guidesY.empty())
    {
        out["editor"]["guidesX"] = JsonNumbers(level.guidesX.data(), (int)level.guidesX.size());
        out["editor"]["guidesY"] = JsonNumbers(level.guidesY.data(), (int)level.guidesY.size());
    }

    return out;
}

bool LevelFromJson(const Json& json, Level& level, std::string& error)
{
    const Json* nodes = json.Find("nodes");
    const Json* world = json.Find("world");

    if (!json.IsObject() || !json.Find("blitlevel"))
    {
        error = "this isn't a level from the Blit level editor";
        return false;
    }

    if (json.Number("blitlevel", 1) > kLevelFormat)
    {
        error = "this level was saved by a newer editor";
        return false;
    }

    level = Level();
    level.is2D = json.Text("mode", "3d") == "2d";
    level.texelsPerMetre = (float)json.Number("texelsPerMetre", level.is2D ? 1.0 : 32.0);
    level.world = PairsFromJson(world ? world->Find("properties") : nullptr);

    if (const Json* editor = json.Find("editor"))
    {
        for (int axis = 0; axis < 2; axis++)
        {
            const Json* guides = editor->Find(axis ? "guidesY" : "guidesX");
            std::vector<float>& into = axis ? level.guidesY : level.guidesX;

            for (size_t i = 0; guides && guides->IsArray() && i < guides->items.size(); i++)
            {
                into.push_back((float)guides->items[i].number);
            }
        }
    }

    for (size_t i = 0; nodes && nodes->IsArray() && i < nodes->items.size(); i++)
    {
        level.nodes.push_back(NodeFromJson(nodes->items[i], level));
    }

    std::set<int> seen;
    ForEachNode(level.nodes, [&](Node& node)
    {
        if (node.id <= 0 || seen.count(node.id))
        {
            node.id = level.nextId++;
        }

        seen.insert(node.id);
    });

    return true;
}

bool SaveLevel(const std::string& path, const Level& level)
{
    return JsonWriteFile(path, LevelToJson(level));
}

bool LoadLevel(const std::string& path, Level& level, std::string& error)
{
    Json json;
    return JsonReadFile(path, json, error) && LevelFromJson(json, level, error);
}
