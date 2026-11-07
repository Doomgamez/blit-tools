#include "render/Scene.h"

#include <cctype>

#include "geometry/Brush.h"

const float kTriggerColor[4] = { 0.3f, 0.9f, 0.4f, 0.3f };
const float kCollisionColor[4] = { 0.95f, 0.55f, 0.2f, 0.3f };
const float kClipColor[4] = { 0.7f, 0.3f, 0.9f, 0.35f };
const float kSkipColor[4] = { 0.5f, 0.5f, 0.5f, 0.2f };
const float kSolid2DColor[4] = { 0.35f, 0.55f, 0.95f, 0.35f };
const float kOpaque[4] = { 1, 1, 1, 1 };

bool SameTextNoCase(const std::string& a, const char* b)
{
    size_t i = 0;

    for (; i < a.size() && b[i]; i++)
    {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    }

    return i == a.size() && !b[i];
}

TriangleBatch& BatchFor(SceneMesh& mesh, GLuint texture, bool translucent)
{
    for (TriangleBatch& batch : mesh.batches)
    {
        if (batch.texture == texture && batch.translucent == translucent) return batch;
    }

    mesh.batches.push_back(TriangleBatch());
    mesh.batches.back().texture = texture;
    mesh.batches.back().translucent = translucent;
    return mesh.batches.back();
}

void AddLine(std::vector<LineVertex>& lines, Vec3 a, Vec3 b, const float color[4])
{
    LineVertex va, vb;
    va.position = a;
    vb.position = b;

    for (int i = 0; i < 4; i++) va.color[i] = vb.color[i] = color[i];

    lines.push_back(va);
    lines.push_back(vb);
}

void AddBox(std::vector<LineVertex>& lines, const Mat4& world, Vec3 lo, Vec3 hi, const float color[4])
{
    Vec3 c[8];

    for (int i = 0; i < 8; i++)
    {
        c[i] = TransformPoint(world, { i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z });
    }

    const int edges[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };

    for (const auto& e : edges) AddLine(lines, c[e[0]], c[e[1]], color);
}

// which colour and texture a face gets, translucent faces aren't what the game draws
void FaceLook(const Element& element, const Face& face, bool is2D, const float*& color, bool& translucent, bool& textured)
{
    color = kOpaque;
    translucent = false;
    textured = true;

    if (is2D) { color = kSolid2DColor; translucent = true; textured = false; }
    if (element.role == BrushRole::Trigger) { color = kTriggerColor; translucent = true; textured = false; }
    else if (element.role == BrushRole::Collision) { color = kCollisionColor; translucent = true; textured = false; }
    else if (SameTextNoCase(face.texture, "clip") || SameTextNoCase(face.texture, "nodraw"))
    {
        color = kClipColor; translucent = true; textured = false;
    }
    else if (SameTextNoCase(face.texture, "skip") || SameTextNoCase(face.texture, "hint") ||
             SameTextNoCase(face.texture, "trigger"))
    {
        color = kSkipColor; translucent = true; textured = false;
    }
}

void AddBrushes(SceneMesh& mesh, const Level& level, const Node&, const Element& element, const Mat4& world,
                SceneContext& context, const SceneOptions& options)
{
    Mat4 normalMatrix = Mat4Transpose(Mat4Inverse(world));

    for (const Brush& brush : element.brushes)
    {
        for (const FacePolygon& polygon : BrushPolygons(brush))
        {
            const Face& face = brush.faces[(size_t)polygon.face];
            const float* color;
            bool translucent, textured;

            FaceLook(element, face, level.is2D, color, translucent, textured);

            // 2D brushes show their front only, the -Z side the 2D camera looks at
            if ((translucent && !options.showVolumes) || (level.is2D && face.normal.z > -0.5f))
            {
                continue;
            }

            const TextureInfo& texture = textured && !face.texture.empty()
                ? GetTexture(*context.textures, face.texture) : TextureInfo{ context.textures->white, 1, 1, true };
            TriangleBatch& batch = BatchFor(mesh, texture.id, translucent);
            Vec3 normal = Normalize(TransformDirection(normalMatrix, face.normal));
            std::vector<DrawVertex> corners;

            for (Vec3 p : polygon.points)
            {
                DrawVertex v = {};
                v.position = TransformPoint(world, p);
                v.normal = normal;
                v.uv = FaceUV(face, p, level.texelsPerMetre, texture.width, texture.height);
                for (int i = 0; i < 4; i++) v.color[i] = color[i];
                corners.push_back(v);
            }

            for (size_t i = 1; i + 1 < corners.size(); i++)
            {
                batch.vertices.push_back(corners[0]);
                batch.vertices.push_back(corners[i]);
                batch.vertices.push_back(corners[i + 1]);
            }

            if (level.is2D)
            {
                float outline[4] = { color[0], color[1], color[2], 0.9f };

                for (size_t i = 0; i < corners.size(); i++)
                {
                    AddLine(mesh.lines, corners[i].position, corners[(i + 1) % corners.size()].position, outline);
                }
            }
        }
    }
}

void AddNodeToScene(SceneMesh& mesh, const Level& level, const Node& node, const Mat4& parent, SceneContext& context,
                    const SceneOptions& options)
{
    if (!node.visible)
    {
        return; // hidden in the editor with its children
    }

    Mat4 world = parent * NodeLocal(node);

    for (const Element& element : node.elements)
    {
        switch (element.type)
        {
        case ElementType::Brush: AddBrushes(mesh, level, node, element, world, context, options); break;
        case ElementType::Model: AddModel(mesh, element, world, context); break;
        case ElementType::Sprite: AddSprite(mesh, element, world, context); break;
        case ElementType::Light:
        {
            SceneLight light;
            light.position = TransformPoint(world, { 0, 0, 0 });
            light.color = element.color / 255.0f;
            light.range = element.range;
            light.intensity = element.intensity;
            mesh.lights.push_back(light);
            break;
        }
        default: break;
        }

        if (options.showIcons)
        {
            AddIcon(mesh, element, world, context.textures->white, options);
        }
    }

    for (const Node& child : node.children)
    {
        AddNodeToScene(mesh, level, child, world, context, options);
    }
}

void BuildScene(SceneMesh& mesh, const Level& level, SceneContext& context, const SceneOptions& options)
{
    mesh.batches.clear();
    mesh.lines.clear();
    mesh.lights.clear();

    for (const Node& node : level.nodes)
    {
        AddNodeToScene(mesh, level, node, Mat4(), context, options);
    }
}
