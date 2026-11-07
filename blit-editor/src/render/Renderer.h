#pragma once

// draws a scene mesh and lines into a framebuffer: textured triangles lit the way the engine lights them (the shadow
// color, a sun and up to 8 point lights, added up and never brighter than the surface, see level/LevelLighting.h),
// then the translucent ones, then lines on top

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
    bool lighting = true;           // off: full bright
    bool flat = false;              // 2D: no normals, the sun everywhere, point lights by distance
    Vec3 shadow{ 0.45f, 0.45f, 0.45f };     // 0-1, what nothing lights
    Vec3 sun{ 0.55f, 0.55f, 0.55f };        // its color times its intensity, black for none
    Vec3 sunToward{ 0.4f, 0.8f, 0.45f };    // toward the sun, length 1
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
