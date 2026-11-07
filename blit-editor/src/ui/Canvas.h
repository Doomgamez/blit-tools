#pragma once

// the 2D view's own tools, like Godot's canvas editor (ROADMAP.md phase 1). they take over from the 3D handles in a
// 2D level's 2D view, for every tool but Rotate:
//
//   click            selects, Ctrl adds to the selection. drag from a node moves the selection, Shift keeps it on one
//                    axis. drag from nothing draws a box and selects what's inside it
//   handles          8 on the selection's rectangle: Shift keeps its shape, Alt resizes from its middle. a box's
//                    brushes change size, anything else is scaled
//   snapping         to other nodes' sides and middles and to guides (Smart in the toolbar), else the grid. a line
//                    shows what it snapped to. Esc or right click puts a drag back
//   arrow keys       nudge a pixel, Shift a grid step
//   Alt right click  a list of everything under the mouse
//   rulers           along the top and left. drag one out for a guide, drag a guide back onto its ruler to remove it
//
// Canvas.cpp clicks and drags, CanvasResize.cpp the handles, CanvasSnap.cpp snapping, CanvasRulers.cpp rulers
// and guides

#include <string>
#include <vector>

#include <imgui.h>

#include "level/Level.h"

struct Editor;

enum class CanvasDragKind { None, Queued, Move, Box, Resize, Guide };

struct CanvasDrag
{
    CanvasDragKind kind = CanvasDragKind::None;
    Vec2 from;                  // the level point it started at
    ImVec2 fromScreen;
    std::string before;         // the level when it started, for Esc
    int handle = -1;            // Resize: 0 top left, then clockwise: top, top right, right, ... left
    int guide = -1;             // Guide: which one, in guidesX (vertical) or guidesY
    bool vertical = false;

    // Move: what moves and where it all was
    std::vector<std::pair<int, Mat4>> moving;
    Vec2 lo, hi;                // the selection's box, or the resized node's rectangle in its own space
    Vec2 primary;               // the primary node's position, what the grid snaps
    // Resize: the node as it was
    Node original;
    Mat4 world;
};

// what snapping found, drawn as lines until the drag ends
struct CanvasSnapLines
{
    bool x = false, y = false;
    float atX = 0, atY = 0;
};

struct CanvasState
{
    CanvasDrag drag;
    CanvasSnapLines lines;
    bool smart = true;          // snap to other nodes and guides, not only the grid
    std::vector<int> under;     // the Alt right click list
};

// a node as the canvas sees it: its rectangle in its own space, or a point when it has no size (a light, a start)
struct CanvasItem
{
    int id = 0;
    Mat4 world;
    bool sized = false;
    bool locked = false;        // can't be clicked, still snapped to
    Vec2 lo, hi;
};

// the game's window, what a camera's frame shows (the engine opens it at this size)
constexpr float kGameWidth = 1280.0f;
constexpr float kGameHeight = 720.0f;

// the 2D view, a 2D level and a tool the canvas does
bool UsesCanvas(const Editor& editor);
// the viewport's input and overlays, origin and size are the viewport image's
void DrawCanvas(Editor& editor, ImVec2 origin, ImVec2 size);
// a drag is going on: the undo snapshot waits for it
bool CanvasBusy(const Editor& editor);

// the pieces
std::vector<CanvasItem> CanvasItems(Editor& editor);
bool ItemRect(Editor& editor, const Node& node, Vec2& lo, Vec2& hi);
// the level point under a pixel of the viewport, and back
Vec2 CanvasToLevel(const Editor& editor, ImVec2 pixel, ImVec2 size);
ImVec2 LevelToCanvas(const Editor& editor, Vec2 point, ImVec2 origin, ImVec2 size);
// the selection's box in the level, the corners of every selected item
bool SelectionRect(Editor& editor, const std::vector<CanvasItem>& items, Vec2& lo, Vec2& hi);
void SetNodeWorld(Editor& editor, Node& node, const Mat4& world);
bool CanvasCancel(Editor& editor);

// CanvasResize.cpp: the handles, clockwise from the top left, and which side each one moves (-1 low, 1 high)
extern const int kHandleX[8];
extern const int kHandleY[8];
const CanvasItem* HandleItem(Editor& editor, const std::vector<CanvasItem>& items);
ImVec2 HandleAt(const Editor& editor, const CanvasItem& item, int handle, ImVec2 origin, ImVec2 size);
int HandleUnder(const Editor& editor, const CanvasItem& item, ImVec2 mouse, ImVec2 origin, ImVec2 size);
ImGuiMouseCursor HandleCursor(int handle);
void DrawHandles(Editor& editor, const CanvasItem& item, int hovered, ImVec2 origin, ImVec2 size);
void StartResize(Editor& editor, const CanvasItem& item, int handle);
void ResizedRect(Editor& editor, const std::vector<CanvasItem>& items, Vec2 mouse, Vec2& lo, Vec2& hi);
void UpdateResize(Editor& editor, const std::vector<CanvasItem>& items, Vec2 mouse);

// CanvasSnap.cpp: the closest of values to a target on one axis within reach, the move that gets it there
bool SnapAxis(const float* values, int count, const std::vector<float>& targets, float reach, float& move, float& at);
// where other nodes' sides and middles and the guides are, leaving out the selection
void SnapTargets(Editor& editor, const std::vector<CanvasItem>& items, std::vector<float>& xs, std::vector<float>& ys);
// a move of the selection, snapped: its box's sides and middle to the targets, else its primary node to the grid
Vec2 SnapMove(Editor& editor, const std::vector<CanvasItem>& items, Vec2 lo, Vec2 hi, Vec2 primary, Vec2 move);
// a dragged point (a handle), snapped on the axes it moves along
Vec2 SnapPoint(Editor& editor, const std::vector<CanvasItem>& items, Vec2 point, bool alongX, bool alongY);
void DrawSnapLines(Editor& editor, ImVec2 origin, ImVec2 size);

// CanvasRulers.cpp: true when the mouse is on a ruler or a guide, so the canvas leaves the click alone
constexpr float kRulerSize = 18.0f;
bool DrawRulers(Editor& editor, ImVec2 origin, ImVec2 size);
void DrawGuides(Editor& editor, ImVec2 origin, ImVec2 size);
