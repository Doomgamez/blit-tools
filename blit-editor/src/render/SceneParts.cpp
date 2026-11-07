// the scene's models, sprites, icons and selection outlines

#include "render/Scene.h"

#include "core/Files.h"
#include "geometry/Brush.h"

const float kSelectedColor[4] = { 1.0f, 0.75f, 0.1f, 1.0f };
const float kPrimaryColor[4] = { 1.0f, 0.95f, 0.4f, 1.0f };

std::string ModelPath(const SceneContext& context, const std::string& file)
{
    return file.empty() ? "" : (FileExists(file) ? file : JoinPath(context.projectRoot, file));
}

void AddModel(SceneMesh& mesh, const Element& element, const Mat4& world, SceneContext& context)
{
    const ModelData& model = GetModel(*context.models, *context.textures, ModelPath(context, element.file));
    Mat4 normalMatrix = Mat4Transpose(Mat4Inverse(world));
    Vec3 tint = element.color / 255.0f;

    for (const ModelPart& part : model.parts)
    {
        TriangleBatch& batch = BatchFor(mesh, part.texture, part.color[3] < 0.99f);

        for (DrawVertex v : part.vertices)
        {
            v.position = TransformPoint(world, v.position);
            v.normal = Normalize(TransformDirection(normalMatrix, v.normal));
            v.color[0] *= tint.x;
            v.color[1] *= tint.y;
            v.color[2] *= tint.z;
            batch.vertices.push_back(v);
        }
    }
}

void AddSprite(SceneMesh& mesh, const Element& element, const Mat4& world, SceneContext& context)
{
    const TextureInfo& texture = GetTexture(*context.textures, element.texture);
    TriangleBatch& batch = BatchFor(mesh, texture.id, true);
    float w = element.size.x, h = element.size.y;
    // the node is the sprite's middle, like the engine's DrawTexturePro with the origin at the centre
    Vec3 corners[4] = { { -w / 2, -h / 2, 0 }, { w / 2, -h / 2, 0 }, { w / 2, h / 2, 0 }, { -w / 2, h / 2, 0 } };
    float repeatU = element.tiled ? w / (float)texture.width : 1.0f;
    float repeatV = element.tiled ? h / (float)texture.height : 1.0f;
    Vec2 uvs[4] = { { 0, 0 }, { repeatU, 0 }, { repeatU, repeatV }, { 0, repeatV } };
    const int order[6] = { 0, 1, 2, 0, 2, 3 };

    for (int i : order)
    {
        DrawVertex v = {};
        v.position = TransformPoint(world, corners[i]);
        // a sprite's depth is its layer, higher layers nearer the 2D camera (which looks along +Z)
        v.position.z -= (float)element.layer * 0.01f;
        v.normal = { 0, 0, -1 };
        v.uv = uvs[i];
        v.color[0] = element.color.x / 255.0f;
        v.color[1] = element.color.y / 255.0f;
        v.color[2] = element.color.z / 255.0f;
        v.color[3] = 1.0f;
        batch.vertices.push_back(v);
    }
}

void IconColor(const Element& element, float out[4])
{
    float r = 0.8f, g = 0.8f, b = 0.8f;

    switch (element.type)
    {
    case ElementType::Light: r = element.color.x / 255.0f; g = element.color.y / 255.0f; b = element.color.z / 255.0f; break;
    case ElementType::Spawn: r = 0.2f; g = 0.9f; b = 0.3f; break;
    case ElementType::Marker: r = 1.0f; g = 0.85f; b = 0.2f; break;
    case ElementType::Camera: r = 0.3f; g = 0.85f; b = 1.0f; break;
    case ElementType::Sound: r = 1.0f; g = 0.45f; b = 0.75f; break;
    case ElementType::Script: r = 0.65f; g = 0.45f; b = 1.0f; break;
    default: break;
    }

    out[0] = r; out[1] = g; out[2] = b; out[3] = 1.0f;
}

bool HasIcon(ElementType type)
{
    return type == ElementType::Light || type == ElementType::Spawn || type == ElementType::Marker ||
           type == ElementType::Camera || type == ElementType::Sound || type == ElementType::Script;
}

void AddIcon(SceneMesh& mesh, const Element& element, const Mat4& world, GLuint white, const SceneOptions& options)
{
    if (!HasIcon(element.type))
    {
        return;
    }

    // a diamond: six points around the node's origin, kept the same size whatever the node's scale
    Vec3 origin = TransformPoint(world, { 0, 0, 0 });
    float s = options.iconSize;
    Vec3 p[6] = { origin + Vec3(s, 0, 0), origin + Vec3(-s, 0, 0), origin + Vec3(0, s, 0),
                  origin + Vec3(0, -s, 0), origin + Vec3(0, 0, s), origin + Vec3(0, 0, -s) };
    const int faces[8][3] = { {0,2,4},{4,2,1},{1,2,5},{5,2,0},{4,3,0},{1,3,4},{5,3,1},{0,3,5} };
    float color[4];
    TriangleBatch& batch = BatchFor(mesh, white, false);

    IconColor(element, color);

    for (const auto& f : faces)
    {
        Vec3 normal = Normalize(Cross(p[f[1]] - p[f[0]], p[f[2]] - p[f[0]]));

        for (int k = 0; k < 3; k++)
        {
            DrawVertex v = {};
            v.position = p[f[k]];
            v.normal = normal;
            for (int c = 0; c < 4; c++) v.color[c] = color[c];
            batch.vertices.push_back(v);
        }
    }

    // where a start or camera faces: yaw 0 looks along +X, like the engine's GetEntityAngles
    if (element.type == ElementType::Spawn || element.type == ElementType::Camera)
    {
        Vec3 forward = Normalize(TransformDirection(world, { 1, 0, 0 }));
        AddLine(mesh.lines, origin, origin + forward * (s * 4.0f), color);
    }
}

bool ElementPickBox(const Element& element, SceneContext& context, const SceneOptions& options, Vec3& lo, Vec3& hi)
{
    switch (element.type)
    {
    case ElementType::Brush:
        return ElementBounds(element, lo, hi);

    case ElementType::Model:
    {
        const ModelData& model = GetModel(*context.models, *context.textures, ModelPath(context, element.file));
        lo = model.boundsMin;
        hi = model.boundsMax;
        return true;
    }

    case ElementType::Sprite:
        lo = { -element.size.x / 2, -element.size.y / 2, -0.5f };
        hi = { element.size.x / 2, element.size.y / 2, 0.5f };
        return true;

    default:
        if (!HasIcon(element.type)) return false;
        lo = { -options.iconSize, -options.iconSize, -options.iconSize };
        hi = { options.iconSize, options.iconSize, options.iconSize };
        return true;
    }
}

void AddNodeOutline(std::vector<LineVertex>& lines, const Level& level, const Node& node, const Mat4& world,
                    const float color[4], SceneContext& context, const SceneOptions& options)
{
    bool drewSomething = false;

    for (const Element& element : node.elements)
    {
        if (element.type == ElementType::Brush)
        {
            for (const Brush& brush : element.brushes)
            {
                for (const FacePolygon& polygon : BrushPolygons(brush))
                {
                    for (size_t i = 0; i < polygon.points.size(); i++)
                    {
                        AddLine(lines, TransformPoint(world, polygon.points[i]),
                                TransformPoint(world, polygon.points[(i + 1) % polygon.points.size()]), color);
                    }
                }
            }

            drewSomething = true;
            continue;
        }

        Vec3 lo, hi;

        if (ElementPickBox(element, context, options, lo, hi))
        {
            // icons keep their size, so their box is drawn around the origin without the node's scale
            Mat4 frame = element.type == ElementType::Model || element.type == ElementType::Sprite
                ? world : Mat4Translate(TransformPoint(world, { 0, 0, 0 }));
            AddBox(lines, frame, lo, hi, color);
            drewSomething = true;
        }

        // a selected light shows how far it reaches, as three circles
        if (element.type == ElementType::Light && !level.is2D)
        {
            Vec3 c = TransformPoint(world, { 0, 0, 0 });
            float dim[4] = { color[0], color[1], color[2], 0.35f };

            for (int i = 0; i < 48; i++)
            {
                float a = i * 2 * kPi / 48, b = (i + 1) * 2 * kPi / 48, r = element.range;
                AddLine(lines, c + Vec3(std::cos(a) * r, 0, std::sin(a) * r), c + Vec3(std::cos(b) * r, 0, std::sin(b) * r), dim);
                AddLine(lines, c + Vec3(std::cos(a) * r, std::sin(a) * r, 0), c + Vec3(std::cos(b) * r, std::sin(b) * r, 0), dim);
                AddLine(lines, c + Vec3(0, std::cos(a) * r, std::sin(a) * r), c + Vec3(0, std::cos(b) * r, std::sin(b) * r), dim);
            }
        }
    }

    // a group shows its origin as a small cross
    if (!drewSomething)
    {
        Vec3 c = TransformPoint(world, { 0, 0, 0 });
        float s = options.iconSize * 1.5f;
        AddLine(lines, c - Vec3(s, 0, 0), c + Vec3(s, 0, 0), color);
        AddLine(lines, c - Vec3(0, s, 0), c + Vec3(0, s, 0), color);
        AddLine(lines, c - Vec3(0, 0, s), c + Vec3(0, 0, s), color);
    }
}

void BuildSelectionLines(std::vector<LineVertex>& lines, const Level& level, const std::vector<int>& selection,
                         SceneContext& context, const SceneOptions& options)
{
    lines.clear();

    for (size_t i = 0; i < selection.size(); i++)
    {
        const Node* node = FindNode(level.nodes, selection[i]);

        if (!node) continue;

        Mat4 world = NodeWorld(level.nodes, node->id);
        AddNodeOutline(lines, level, *node, world, i + 1 == selection.size() ? kPrimaryColor : kSelectedColor,
                       context, options);

        // children of a selected node are part of it
        ForEachNode(node->children, [&](const Node& child)
        {
            AddNodeOutline(lines, level, child, NodeWorld(level.nodes, child.id), kSelectedColor, context, options);
        });
    }
}
