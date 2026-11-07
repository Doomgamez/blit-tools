// the 2D view's rulers along the top and left, and the guides dragged out of them. a guide is a line at an x (from
// the left ruler) or a y (from the top one) that moved things snap to, kept in the level for the editor only

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ui/Canvas.h"
#include "ui/Editor.h"

const ImU32 kRulerBack = IM_COL32(32, 34, 40, 245);
const ImU32 kRulerTick = IM_COL32(150, 155, 165, 255);
const ImU32 kGuideColor = IM_COL32(80, 200, 255, 170);
const ImU32 kGuideHot = IM_COL32(140, 230, 255, 255);
const float kGuideReach = 4.0f;     // pixels

// a round step for the ruler's numbers: 1, 2 or 5 times a power of ten, about a hundred pixels apart
float RulerStep(float zoom)
{
    float raw = 100.0f * zoom;
    float power = std::pow(10.0f, std::floor(std::log10(raw)));
    float n = raw / power;
    return (n < 1.5f ? 1.0f : n < 3.5f ? 2.0f : n < 7.5f ? 5.0f : 10.0f) * power;
}

void DrawRulerTicks(Editor& editor, ImVec2 origin, ImVec2 size, bool vertical)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float step = RulerStep(editor.camera.zoom), minor = step / 5.0f;
    Vec2 a = CanvasToLevel(editor, ImVec2(kRulerSize, kRulerSize), size);
    Vec2 b = CanvasToLevel(editor, size, size);
    float from = vertical ? a.y : a.x, to = vertical ? b.y : b.x;
    bool showMinor = minor / editor.camera.zoom >= 5.0f;

    for (float v = std::floor(from / minor) * minor; v <= to; v += minor)
    {
        bool major = std::fabs(std::remainder(v, step)) < minor * 0.5f;
        ImVec2 at = LevelToCanvas(editor, { vertical ? 0 : v, vertical ? v : 0 }, origin, size);
        float length = major ? kRulerSize : kRulerSize * 0.3f;

        if (!major && !showMinor) continue;

        if (vertical)
        {
            if (at.y < origin.y + kRulerSize) continue;
            draw->AddLine(ImVec2(origin.x + kRulerSize - length, at.y), ImVec2(origin.x + kRulerSize, at.y), kRulerTick);
        }
        else
        {
            if (at.x < origin.x + kRulerSize) continue;
            draw->AddLine(ImVec2(at.x, origin.y + kRulerSize - length), ImVec2(at.x, origin.y + kRulerSize), kRulerTick);
        }

        if (!major) continue;

        char label[32];
        snprintf(label, sizeof(label), "%g", std::fabs(v) < minor * 0.01f ? 0.0f : v);

        if (!vertical)
        {
            draw->AddText(ImVec2(at.x + 3, origin.y + 1), kRulerTick, label);
            continue;
        }

        // the left ruler is narrow: its numbers go down one character at a time
        float y = at.y + 2;
        for (const char* c = label; *c; c++, y += ImGui::GetFontSize() - 3)
        {
            char one[2] = { *c, 0 };
            draw->AddText(ImVec2(origin.x + 4, y), kRulerTick, one);
        }
    }
}

bool InTopRuler(ImVec2 mouse, ImVec2 origin, ImVec2 size)
{
    return mouse.x >= origin.x && mouse.x < origin.x + size.x && mouse.y >= origin.y && mouse.y < origin.y + kRulerSize;
}

bool InLeftRuler(ImVec2 mouse, ImVec2 origin, ImVec2 size)
{
    return mouse.x >= origin.x && mouse.x < origin.x + kRulerSize && mouse.y >= origin.y && mouse.y < origin.y + size.y;
}

// the guide under the mouse, -1 for none
int GuideUnder(Editor& editor, ImVec2 mouse, ImVec2 origin, ImVec2 size, bool& vertical)
{
    const Level& level = editor.doc.level;

    for (size_t i = 0; i < level.guidesX.size(); i++)
    {
        vertical = true;
        if (std::fabs(LevelToCanvas(editor, { level.guidesX[i], 0 }, origin, size).x - mouse.x) < kGuideReach) return (int)i;
    }

    for (size_t i = 0; i < level.guidesY.size(); i++)
    {
        vertical = false;
        if (std::fabs(LevelToCanvas(editor, { 0, level.guidesY[i] }, origin, size).y - mouse.y) < kGuideReach) return (int)i;
    }

    return -1;
}

void DrawGuides(Editor& editor, ImVec2 origin, ImVec2 size)
{
    const Level& level = editor.doc.level;
    const CanvasDrag& drag = editor.canvas.drag;
    ImDrawList* draw = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < level.guidesX.size(); i++)
    {
        bool hot = drag.kind == CanvasDragKind::Guide && drag.vertical && drag.guide == (int)i;
        float x = LevelToCanvas(editor, { level.guidesX[i], 0 }, origin, size).x;
        draw->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + size.y), hot ? kGuideHot : kGuideColor);
    }

    for (size_t i = 0; i < level.guidesY.size(); i++)
    {
        bool hot = drag.kind == CanvasDragKind::Guide && !drag.vertical && drag.guide == (int)i;
        float y = LevelToCanvas(editor, { 0, level.guidesY[i] }, origin, size).y;
        draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + size.x, y), hot ? kGuideHot : kGuideColor);
    }
}

// a guide being dragged follows the mouse, on the grid when Snap is on. let go on its ruler and it's gone
void DragGuide(Editor& editor, ImVec2 mouse, ImVec2 origin, ImVec2 size, Vec2 at)
{
    CanvasDrag& drag = editor.canvas.drag;
    std::vector<float>& guides = drag.vertical ? editor.doc.level.guidesX : editor.doc.level.guidesY;

    if (drag.guide < 0 || drag.guide >= (int)guides.size())
    {
        drag = CanvasDrag();
        return;
    }

    float value = drag.vertical ? at.x : at.y;
    guides[(size_t)drag.guide] = editor.snap ? Snap(value, GridStep(editor)) : value;
    ImGui::SetMouseCursor(drag.vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    editor.doc.Changed();

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        bool away = drag.vertical ? InLeftRuler(mouse, origin, size) || mouse.x < origin.x
                                  : InTopRuler(mouse, origin, size) || mouse.y < origin.y;

        if (away) guides.erase(guides.begin() + drag.guide);
        drag = CanvasDrag();
    }
}

bool DrawRulers(Editor& editor, ImVec2 origin, ImVec2 size)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    CanvasDrag& drag = editor.canvas.drag;
    ImVec2 mouse = ImGui::GetIO().MousePos;
    Vec2 at = CanvasToLevel(editor, ImVec2(mouse.x - origin.x, mouse.y - origin.y), size);
    bool top = InTopRuler(mouse, origin, size), left = InLeftRuler(mouse, origin, size);
    bool vertical = false;
    int guide = top || left ? -1 : GuideUnder(editor, mouse, origin, size, vertical);

    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + kRulerSize), kRulerBack);
    draw->AddRectFilled(origin, ImVec2(origin.x + kRulerSize, origin.y + size.y), kRulerBack);
    DrawRulerTicks(editor, origin, size, false);
    DrawRulerTicks(editor, origin, size, true);

    // where the mouse is, on both
    if (editor.viewportHovered)
    {
        draw->AddLine(ImVec2(mouse.x, origin.y), ImVec2(mouse.x, origin.y + kRulerSize), kGuideHot);
        draw->AddLine(ImVec2(origin.x, mouse.y), ImVec2(origin.x + kRulerSize, mouse.y), kGuideHot);
    }

    if (drag.kind == CanvasDragKind::Guide)
    {
        DragGuide(editor, mouse, origin, size, at);
        return true;
    }

    if (!editor.viewportHovered || drag.kind != CanvasDragKind::None) return false;

    if (guide >= 0) ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (top || left || guide >= 0))
    {
        drag = CanvasDrag();
        drag.kind = CanvasDragKind::Guide;
        drag.before = editor.doc.Snapshot();

        if (guide >= 0)
        {
            drag.guide = guide;
            drag.vertical = vertical;
        }
        else
        {
            // out of the top ruler comes a horizontal guide, out of the left one a vertical guide
            drag.vertical = left && !top;
            std::vector<float>& guides = drag.vertical ? editor.doc.level.guidesX : editor.doc.level.guidesY;
            guides.push_back(drag.vertical ? at.x : at.y);
            drag.guide = (int)guides.size() - 1;
        }
    }

    return top || left || guide >= 0;
}
