// the canvas's 8 handles on a single selected node, and resizing with them, see Canvas.h

#include <algorithm>
#include <cmath>

#include "geometry/Brush.h"
#include "ui/Canvas.h"
#include "ui/Editor.h"

// the handles' corners and middles, clockwise from the top left, and which side of the rectangle each one moves
extern const int kHandleX[8] = { -1, 0, 1, 1, 1, 0, -1, -1 };
extern const int kHandleY[8] = { -1, -1, -1, 0, 1, 1, 1, 0 };
extern const float kHandleSize = 7.0f;
extern const ImU32 kHandleColor = IM_COL32(255, 200, 40, 255);

// the one item the handles are on: a single sized selection
const CanvasItem* HandleItem(Editor& editor, const std::vector<CanvasItem>& items)
{
    std::vector<int> top = editor.doc.TopSelection();

    for (const CanvasItem& item : items)
    {
        if (top.size() == 1 && item.id == top[0] && item.sized && !item.locked) return &item;
    }

    return nullptr;
}

ImVec2 HandleAt(const Editor& editor, const CanvasItem& item, int handle, ImVec2 origin, ImVec2 size)
{
    float x = kHandleX[handle] < 0 ? item.lo.x : kHandleX[handle] > 0 ? item.hi.x : (item.lo.x + item.hi.x) / 2;
    float y = kHandleY[handle] < 0 ? item.lo.y : kHandleY[handle] > 0 ? item.hi.y : (item.lo.y + item.hi.y) / 2;
    Vec3 p = TransformPoint(item.world, { x, y, 0 });
    return LevelToCanvas(editor, { p.x, p.y }, origin, size);
}

int HandleUnder(const Editor& editor, const CanvasItem& item, ImVec2 mouse, ImVec2 origin, ImVec2 size)
{
    for (int h = 0; h < 8; h++)
    {
        ImVec2 at = HandleAt(editor, item, h, origin, size);
        if (std::fabs(at.x - mouse.x) <= kHandleSize && std::fabs(at.y - mouse.y) <= kHandleSize) return h;
    }

    return -1;
}

ImGuiMouseCursor HandleCursor(int handle)
{
    if (kHandleX[handle] == 0) return ImGuiMouseCursor_ResizeNS;
    if (kHandleY[handle] == 0) return ImGuiMouseCursor_ResizeEW;
    return kHandleX[handle] == kHandleY[handle] ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_ResizeNESW;
}

void DrawHandles(Editor& editor, const CanvasItem& item, int hovered, ImVec2 origin, ImVec2 size)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // over the rectangle the selection outline already draws
    for (int h = 0; h < 8; h++)
    {
        ImVec2 p = HandleAt(editor, item, h, origin, size);
        float r = kHandleSize / 2 + (h == hovered || h == editor.canvas.drag.handle ? 1.5f : 0.0f);
        draw->AddRectFilled(ImVec2(p.x - r, p.y - r), ImVec2(p.x + r, p.y + r), IM_COL32(30, 30, 36, 255));
        draw->AddRect(ImVec2(p.x - r, p.y - r), ImVec2(p.x + r, p.y + r), kHandleColor);
    }
}

void StartResize(Editor& editor, const CanvasItem& item, int handle)
{
    CanvasDrag& drag = editor.canvas.drag;
    drag.kind = CanvasDragKind::Resize;
    drag.handle = handle;
    drag.original = *FindNode(editor.doc.level.nodes, item.id);
    drag.world = item.world;
    drag.lo = item.lo;
    drag.hi = item.hi;
}

// the new rectangle, in the node's own space, from where the handle is dragged to
void ResizedRect(Editor& editor, const std::vector<CanvasItem>& items, Vec2 mouse, Vec2& lo, Vec2& hi)
{
    CanvasDrag& drag = editor.canvas.drag;
    int sx = kHandleX[drag.handle], sy = kHandleY[drag.handle];
    Vec2 middle = { (drag.lo.x + drag.hi.x) / 2, (drag.lo.y + drag.hi.y) / 2 };
    Vec3 start = TransformPoint(drag.world, { sx < 0 ? drag.lo.x : sx > 0 ? drag.hi.x : middle.x,
                                              sy < 0 ? drag.lo.y : sy > 0 ? drag.hi.y : middle.y, 0 });
    Vec2 target = { start.x + mouse.x - drag.from.x, start.y + mouse.y - drag.from.y };
    Vec3 turned = TransformDirection(drag.world, { 1, 0, 0 });

    // snapping is along the level's axes, so only for nodes that aren't turned
    if (std::fabs(turned.y) < 1e-4f) target = SnapPoint(editor, items, target, sx != 0, sy != 0);

    Vec3 local = TransformPoint(Mat4Inverse(drag.world), { target.x, target.y, 0 });
    float w = drag.hi.x - drag.lo.x, h = drag.hi.y - drag.lo.y;
    bool symmetric = ImGui::GetIO().KeyAlt;
    lo = drag.lo;
    hi = drag.hi;

    if (sx < 0) lo.x = std::min(local.x, drag.hi.x - 1.0f);
    if (sx > 0) hi.x = std::max(local.x, drag.lo.x + 1.0f);
    if (sy < 0) lo.y = std::min(local.y, drag.hi.y - 1.0f);
    if (sy > 0) hi.y = std::max(local.y, drag.lo.y + 1.0f);

    // Alt: the other side moves the other way, the middle stays
    if (symmetric)
    {
        if (sx < 0) hi.x = 2 * middle.x - lo.x;
        if (sx > 0) lo.x = 2 * middle.x - hi.x;
        if (sy < 0) hi.y = 2 * middle.y - lo.y;
        if (sy > 0) lo.y = 2 * middle.y - hi.y;
    }

    // Shift: the same shape. a side drags the other axis along around its middle, a corner takes the bigger stretch
    if (ImGui::GetIO().KeyShift && w > 0 && h > 0)
    {
        float k = sx && sy ? std::max((hi.x - lo.x) / w, (hi.y - lo.y) / h) : sx ? (hi.x - lo.x) / w : (hi.y - lo.y) / h;
        float nw = w * k, nh = h * k;

        if (sx < 0) lo.x = symmetric ? middle.x - nw / 2 : hi.x - nw;
        else if (sx > 0) hi.x = symmetric ? middle.x + nw / 2 : lo.x + nw;
        else { lo.x = middle.x - nw / 2; hi.x = middle.x + nw / 2; }

        if (sy < 0) lo.y = symmetric ? middle.y - nh / 2 : hi.y - nh;
        else if (sy > 0) hi.y = symmetric ? middle.y + nh / 2 : lo.y + nh;
        else { lo.y = middle.y - nh / 2; hi.y = middle.y + nh / 2; }
    }
}

void UpdateResize(Editor& editor, const std::vector<CanvasItem>& items, Vec2 mouse)
{
    CanvasDrag& drag = editor.canvas.drag;
    Node* node = FindNode(editor.doc.level.nodes, drag.original.id);
    Vec2 lo, hi;

    if (!node) return;

    ResizedRect(editor, items, mouse, lo, hi);
    node->elements = drag.original.elements;
    node->position = drag.original.position;
    node->scale = drag.original.scale;

    if (HasElement(*node, ElementType::Brush))
    {
        // a box changes size, its sprite and textures follow. the depth stays as it is
        for (Element& element : node->elements)
        {
            for (Brush& brush : element.brushes)
            {
                FitBrush(brush, { drag.lo.x, drag.lo.y, -1e4f }, { drag.hi.x, drag.hi.y, 1e4f }, { lo.x, lo.y, -1e4f },
                         { hi.x, hi.y, 1e4f });
            }
        }
    }
    else
    {
        // anything else is scaled, the side across from the handle stays where it is
        float kx = (hi.x - lo.x) / std::max(drag.hi.x - drag.lo.x, 1e-4f);
        float ky = (hi.y - lo.y) / std::max(drag.hi.y - drag.lo.y, 1e-4f);
        Vec3 shift = TransformDirection(NodeLocal(drag.original), { lo.x - kx * drag.lo.x, lo.y - ky * drag.lo.y, 0 });
        node->scale = { drag.original.scale.x * kx, drag.original.scale.y * ky, drag.original.scale.z };
        node->position = drag.original.position + shift;
    }

    editor.doc.Changed();
}
