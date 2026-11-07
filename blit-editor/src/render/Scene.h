#pragma once

// the level as triangles and lines for the viewport, in level space. rebuilt when the document's version changes,
// the selection's outlines are rebuilt every frame

#include <string>
#include <vector>

#include "level/Level.h"
#include "render/Models.h"
#include "render/Textures.h"

struct TriangleBatch
{
    GLuint texture = 0;
    bool translucent = false;       // volumes and special faces, drawn after the solids without writing depth
    float order = 0;                // 2D: its node's z in the level, translucent batches are drawn lowest first
    std::vector<DrawVertex> vertices;
};

struct SceneLight
{
    Vec3 position;
    Vec3 color;                     // 0-1
    float range = 6.0f;
    float intensity = 1.0f;
};

struct SceneOptions
{
    bool showVolumes = true;        // triggers, collision and clip faces
    bool showIcons = true;          // lights, starts, markers...
    float iconSize = 0.2f;          // metres, or pixels in 2D
};

struct SceneMesh
{
    int version = -1;
    // while it's built: a 2D level keeps the tree's order, each node's batches go in at its z
    bool ordered = false;
    float order = 0;
    std::vector<TriangleBatch> batches;
    std::vector<LineVertex> lines;  // outlines of 2D brushes, direction arrows
    std::vector<SceneLight> lights; // for the preview lighting
};

// what the scene needs to find files: the project's root (model paths are relative to it)
struct SceneContext
{
    TextureCache* textures = nullptr;
    ModelCache* models = nullptr;
    std::string projectRoot;
};

void BuildScene(SceneMesh& mesh, const Level& level, SceneContext& context, const SceneOptions& options);

// outlines of the selected nodes (and faces) for this frame
void BuildSelectionLines(std::vector<LineVertex>& lines, const Level& level, const std::vector<int>& selection,
                         SceneContext& context, const SceneOptions& options);

// the pieces
TriangleBatch& BatchFor(SceneMesh& mesh, GLuint texture, bool translucent);
void AddLine(std::vector<LineVertex>& lines, Vec3 a, Vec3 b, const float color[4]);
void AddBox(std::vector<LineVertex>& lines, const Mat4& world, Vec3 lo, Vec3 hi, const float color[4]);
void AddBrushes(SceneMesh& mesh, const Level& level, const Node& node, const Element& element, const Mat4& world,
                SceneContext& context, const SceneOptions& options);
void AddModel(SceneMesh& mesh, const Element& element, const Mat4& world, SceneContext& context);
void AddSprite(SceneMesh& mesh, const Node& node, const Element& element, const Mat4& world, SceneContext& context);
void AddIcon(SceneMesh& mesh, const Element& element, const Mat4& world, GLuint white, const SceneOptions& options);
void AddNodeToScene(SceneMesh& mesh, const Level& level, const Node& node, const Mat4& parent, SceneContext& context,
                    const SceneOptions& options);
bool SameTextNoCase(const std::string& a, const char* b);
// which colour and texture a face gets, translucent faces aren't what the game draws
void FaceLook(const Element& element, const Face& face, bool is2D, const float*& color, bool& translucent, bool& textured);
// the colour of an element's icon, and whether it has one (lights, starts, markers, cameras, sounds, scripts)
void IconColor(const Element& element, float out[4]);
bool HasIcon(ElementType type);
// a model element's file as a full path: it's relative to the project's root
std::string ModelPath(const SceneContext& context, const std::string& file);
void AddNodeOutline(std::vector<LineVertex>& lines, const Level& level, const Node& node, const Mat4& world,
                    const float color[4], SceneContext& context, const SceneOptions& options);
// the box an element's icon or model takes up in its node's space, for picking and outlines
bool ElementPickBox(const Node& node, const Element& element, SceneContext& context, const SceneOptions& options,
                    Vec3& lo, Vec3& hi);
// about where a text is in its node's space: its lines at its size, placed by its alignment (render/TextArea.cpp)
void TextArea(const Element& text, Vec3& lo, Vec3& hi);
// how many lines and the longest one's characters
void TextLines(const std::string& text, int& lines, int& longest);
// what a sprite covers in its node's space: the node's brushes, else its picture's size in pixels, centred
void SpriteArea(const Node& node, const Element& sprite, SceneContext& context, Vec3& lo, Vec3& hi);
