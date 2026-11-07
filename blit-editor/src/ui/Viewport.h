#pragma once

// the viewport's pieces: Viewport.cpp draws and moves the camera, ViewportTools.cpp picks, drags handles and takes
// textures and models dropped from the assets panel

#include <imgui.h>

#include "ui/Editor.h"

extern const char* const kTexturePayload;   // a texture name dragged from the assets panel
extern const char* const kModelPayload;     // a model file
extern const char* const kClassPayload;     // a level object class name

void AddGridLines(Editor& editor, std::vector<LineVertex>& lines);
void MoveCamera(Editor& editor, ImVec2 mouse, ImVec2 size);
void RenderLevel(Editor& editor, const Mat4& view, const Mat4& projection);

struct PickHit
{
    int node = 0;
    float distance = 1e30f;
    FaceRef face;               // when it hit a brush
};

// the nearest visible, unlocked thing under the ray
PickHit PickAt(Editor& editor, const Ray& ray);
void PickNode(Editor& editor, const Node& node, const Mat4& parent, const Ray& ray, PickHit& best);
void ClickSelect(Editor& editor, const Ray& ray, bool additive);
// the selected faces' outlines, in face mode
void AddFaceOutlines(Editor& editor, std::vector<LineVertex>& lines);

// the move / rotate / scale / resize handles, true when the mouse is on them or dragging them
bool DrawGizmo(Editor& editor, const Mat4& view, const Mat4& projection, ImVec2 origin, ImVec2 size);
bool DrawResizeGizmo(Editor& editor, Node& node, const Mat4& view, const Mat4& projection);
// a level point to the viewport's pixels, false behind the camera
bool ToScreen(const Mat4& viewProjection, Vec3 point, ImVec2 origin, ImVec2 size, ImVec2& out);
// each script node's class picture over it, facing the camera
void DrawClassPictures(Editor& editor, const Mat4& view, const Mat4& projection, ImVec2 origin, ImVec2 size);
// a class dropped on a node gives it that script, dropped on nothing it's a new script object
void DropClass(Editor& editor, const std::string& className, const Ray& ray, ImVec2 mouse, ImVec2 size);
void HandleViewportDrops(Editor& editor, const Ray& ray, ImVec2 mouse, ImVec2 size);
// TextNodes.cpp: every Text element's words where the game puts them
void DrawTextNodes(Editor& editor, const Mat4& view, const Mat4& projection, ImVec2 origin, ImVec2 size);
