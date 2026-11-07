#pragma once

// draws a scene mesh and lines into a framebuffer: textured triangles lit like a rough preview of the engine
// (ambient, a sun and up to 8 point lights), then the translucent ones, then lines on top

#include <vector>

#include "render/Gpu.h"
#include "render/Scene.h"

constexpr int kMaxPreviewLights = 8;

struct Renderer
{
    Shader lit;
    Shader flat;
    GLuint triangleBuffer = 0;
    GLuint lineBuffer = 0;
    std::string error;
};

struct DrawSettings
{
    Mat4 viewProjection;
    Vec3 eye;                       // the nearest lights to it are used
    bool lighting = true;           // off: full bright, how 2D levels are shown
    float ambient = 0.35f;
    float background[4] = { 0.11f, 0.12f, 0.14f, 1.0f };
};

bool InitRenderer(Renderer& renderer);
void FreeRenderer(Renderer& renderer);

void BeginFrame(Renderer& renderer, Framebuffer& target, const DrawSettings& settings);
void DrawScene(Renderer& renderer, const SceneMesh& mesh, const DrawSettings& settings);
// lines over everything, depthTest: hidden behind the level (the grid), else always on top (the selection)
void DrawLines(Renderer& renderer, const std::vector<LineVertex>& lines, const DrawSettings& settings, bool depthTest);
void EndFrame(Renderer& renderer);

// the pieces
void DrawBatch(Renderer& renderer, const TriangleBatch& batch);
void SetLights(Renderer& renderer, const SceneMesh& mesh, const DrawSettings& settings);
