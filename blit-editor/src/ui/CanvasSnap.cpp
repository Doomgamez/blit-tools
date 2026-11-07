// the canvas's snapping, like Godot's smart snap: a moved or resized side goes to another node's side or middle, or
// a guide, when it comes within a few pixels of one. otherwise the grid, when Snap is on

#include <cmath>

#include "ui/Canvas.h"
#include "ui/Editor.h"

const float kSnapReach = 8.0f;      // pixels on screen
const ImU32 kSnapColor = IM_COL32(255, 90, 200, 220);

// selected, or inside something selected: it moves along, so it's no target
bool MovesWithSelection(Editor& editor, int id)
{
    for (int at = id; at > 0; at = FindParentId(editor.doc.level.nodes, at))
    {
        if (editor.doc.IsSelected(at)) return true;
    }

    return false;
}

void SnapTargets(Editor& editor, const std::vector<CanvasItem>& items, std::vector<float>& xs, std::vector<float>& ys)
{
    const Level& level = editor.doc.level;
    xs.assign(level.guidesX.begin(), level.guidesX.end());
    ys.assign(level.guidesY.begin(), level.guidesY.end());

    for (const CanvasItem& item : items)
    {
        // a turned node's sides aren't along the level's axes
        if (std::fabs(TransformDirection(item.world, { 1, 0, 0 }).y) > 1e-4f || MovesWithSelection(editor, item.id)) continue;

        Vec3 a = TransformPoint(item.world, item.sized ? Vec3(item.lo.x, item.lo.y, 0) : Vec3(0, 0, 0));
        Vec3 b = TransformPoint(item.world, item.sized ? Vec3(item.hi.x, item.hi.y, 0) : Vec3(0, 0, 0));
        xs.insert(xs.end(), { a.x, b.x, (a.x + b.x) / 2 });
        ys.insert(ys.end(), { a.y, b.y, (a.y + b.y) / 2 });
    }
}

bool SnapAxis(const float* values, int count, const std::vector<float>& targets, float reach, float& move, float& at)
{
    bool found = false;

    for (int i = 0; i < count; i++)
    {
        for (float target : targets)
        {
            float d = target - values[i];

            if (std::fabs(d) < reach)
            {
                reach = std::fabs(d);
                move = d;
                at = target;
                found = true;
            }
        }
    }

    return found;
}

Vec2 SnapMove(Editor& editor, const std::vector<CanvasItem>& items, Vec2 lo, Vec2 hi, Vec2 primary, Vec2 move)
{
    CanvasSnapLines& lines = editor.canvas.lines;
    std::vector<float> xs, ys;
    float reach = kSnapReach * editor.camera.zoom, grid = GridStep(editor), d = 0;

    lines = CanvasSnapLines();
    if (editor.canvas.smart) SnapTargets(editor, items, xs, ys);

    float sideX[3] = { lo.x + move.x, hi.x + move.x, (lo.x + hi.x) / 2 + move.x };
    float sideY[3] = { lo.y + move.y, hi.y + move.y, (lo.y + hi.y) / 2 + move.y };

    // an axis Shift holds still stays still
    if (move.x != 0 && SnapAxis(sideX, 3, xs, reach, d, lines.atX)) { move.x += d; lines.x = true; }
    else if (move.x != 0 && editor.snap) move.x = Snap(primary.x + move.x, grid) - primary.x;

    if (move.y != 0 && SnapAxis(sideY, 3, ys, reach, d, lines.atY)) { move.y += d; lines.y = true; }
    else if (move.y != 0 && editor.snap) move.y = Snap(primary.y + move.y, grid) - primary.y;

    return move;
}

Vec2 SnapPoint(Editor& editor, const std::vector<CanvasItem>& items, Vec2 point, bool alongX, bool alongY)
{
    CanvasSnapLines& lines = editor.canvas.lines;
    std::vector<float> xs, ys;
    float reach = kSnapReach * editor.camera.zoom, grid = GridStep(editor), d = 0;

    lines = CanvasSnapLines();
    if (editor.canvas.smart) SnapTargets(editor, items, xs, ys);

    if (alongX && SnapAxis(&point.x, 1, xs, reach, d, lines.atX)) { point.x += d; lines.x = true; }
    else if (alongX && editor.snap) point.x = Snap(point.x, grid);

    if (alongY && SnapAxis(&point.y, 1, ys, reach, d, lines.atY)) { point.y += d; lines.y = true; }
    else if (alongY && editor.snap) point.y = Snap(point.y, grid);

    return point;
}

void DrawSnapLines(Editor& editor, ImVec2 origin, ImVec2 size)
{
    const CanvasSnapLines& lines = editor.canvas.lines;
    ImDrawList* draw = ImGui::GetWindowDrawList();

    if (!CanvasBusy(editor)) return;

    if (lines.x)
    {
        float x = LevelToCanvas(editor, { lines.atX, 0 }, origin, size).x;
        draw->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + size.y), kSnapColor, 1.5f);
    }

    if (lines.y)
    {
        float y = LevelToCanvas(editor, { 0, lines.atY }, origin, size).y;
        draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + size.x, y), kSnapColor, 1.5f);
    }
}
