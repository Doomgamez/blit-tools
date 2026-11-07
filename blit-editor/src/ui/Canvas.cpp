// the 2D view's clicks, drags and handles, see Canvas.h

#include "ui/Canvas.h"

#include <algorithm>
#include <cmath>

#include "geometry/Brush.h"
#include "ui/Editor.h"
#include "ui/Viewport.h"

bool UsesCanvas(const Editor& editor)
{
    return editor.doc.level.is2D && editor.camera.kind == ViewKind::TwoD && editor.tool != Tool::Rotate &&
           !editor.faceMode;
}

bool CanvasBusy(const Editor& editor)
{
    CanvasDragKind kind = editor.canvas.drag.kind;
    return kind != CanvasDragKind::None && kind != CanvasDragKind::Queued;
}

Vec2 CanvasToLevel(const Editor& editor, ImVec2 pixel, ImVec2 size)
{
    Vec3 at = CameraRay(editor.camera, pixel.x, pixel.y, size.x, size.y).origin;
    return { at.x, at.y };
}

ImVec2 LevelToCanvas(const Editor& editor, Vec2 point, ImVec2 origin, ImVec2 size)
{
    Mat4 viewProjection = CameraProjection(editor.camera, size.x, size.y) * CameraView(editor.camera);
    ImVec2 out;
    ToScreen(viewProjection, { point.x, point.y, 0 }, origin, size, out);
    return out;
}

bool ItemRect(Editor& editor, const Node& node, Vec2& lo, Vec2& hi)
{
    SceneContext context = MakeSceneContext(editor);
    bool any = false;

    for (const Element& element : node.elements)
    {
        Vec3 a, b;
        bool sized = element.type == ElementType::Brush || element.type == ElementType::Sprite ||
                     element.type == ElementType::Model || element.type == ElementType::Text;

        if (sized && ElementPickBox(node, element, context, editor.sceneOptions, a, b))
        {
            lo = any ? Vec2{ std::min(lo.x, a.x), std::min(lo.y, a.y) } : Vec2{ a.x, a.y };
            hi = any ? Vec2{ std::max(hi.x, b.x), std::max(hi.y, b.y) } : Vec2{ b.x, b.y };
            any = true;
        }
    }

    return any;
}

void AddCanvasItems(Editor& editor, const std::vector<Node>& nodes, const Mat4& parent, std::vector<CanvasItem>& out)
{
    for (const Node& node : nodes)
    {
        if (!node.visible) continue;

        CanvasItem item;
        item.id = node.id;
        item.world = parent * NodeLocal(node);
        item.locked = node.locked;
        item.sized = ItemRect(editor, node, item.lo, item.hi);
        out.push_back(item);
        AddCanvasItems(editor, node.children, item.world, out);
    }
}

std::vector<CanvasItem> CanvasItems(Editor& editor)
{
    std::vector<CanvasItem> items;
    AddCanvasItems(editor, editor.doc.level.nodes, Mat4(), items);
    return items;
}

// the item's corners in the level, top left first and clockwise. a point item's four are its position
void ItemCorners(const CanvasItem& item, Vec2 out[4])
{
    Vec2 local[4] = { item.lo, { item.hi.x, item.lo.y }, item.hi, { item.lo.x, item.hi.y } };

    for (int i = 0; i < 4; i++)
    {
        Vec3 p = TransformPoint(item.world, item.sized ? Vec3(local[i].x, local[i].y, 0) : Vec3(0, 0, 0));
        out[i] = { p.x, p.y };
    }
}

bool SelectionRect(Editor& editor, const std::vector<CanvasItem>& items, Vec2& lo, Vec2& hi)
{
    bool any = false;

    for (const CanvasItem& item : items)
    {
        if (!editor.doc.IsSelected(item.id)) continue;

        Vec2 corners[4];
        ItemCorners(item, corners);

        for (const Vec2& c : corners)
        {
            lo = any ? Vec2{ std::min(lo.x, c.x), std::min(lo.y, c.y) } : c;
            hi = any ? Vec2{ std::max(hi.x, c.x), std::max(hi.y, c.y) } : c;
            any = true;
        }
    }

    return any;
}

void SetNodeWorld(Editor& editor, Node& node, const Mat4& world)
{
    int parent = FindParentId(editor.doc.level.nodes, node.id);
    Mat4 parentWorld = parent > 0 ? NodeWorld(editor.doc.level.nodes, parent) : Mat4();
    Mat4Decompose(Mat4Inverse(parentWorld) * world, node.position, node.rotation, node.scale);
}

bool CanvasCancel(Editor& editor)
{
    CanvasDrag& drag = editor.canvas.drag;
    bool busy = CanvasBusy(editor);

    if (drag.kind == CanvasDragKind::Move || drag.kind == CanvasDragKind::Resize || drag.kind == CanvasDragKind::Guide)
    {
        editor.doc.Restore(drag.before);
    }

    drag = CanvasDrag();
    editor.canvas.lines = CanvasSnapLines();
    return busy;
}

void StartMove(Editor& editor, const std::vector<CanvasItem>& items)
{
    CanvasDrag& drag = editor.canvas.drag;
    drag.kind = CanvasDragKind::Move;
    drag.moving.clear();

    for (int id : editor.doc.TopSelection())
    {
        if (const Node* node = FindNode(editor.doc.level.nodes, id))
        {
            if (!node->locked) drag.moving.push_back({ id, NodeWorld(editor.doc.level.nodes, id) });
        }
    }

    SelectionRect(editor, items, drag.lo, drag.hi);
    Vec3 primary = drag.moving.empty() ? Vec3() : TransformPoint(drag.moving.back().second, { 0, 0, 0 });
    drag.primary = { primary.x, primary.y };
}

void UpdateMove(Editor& editor, const std::vector<CanvasItem>& items, Vec2 mouse)
{
    CanvasDrag& drag = editor.canvas.drag;
    Vec2 move = { mouse.x - drag.from.x, mouse.y - drag.from.y };

    // Shift: only along the axis it's gone furthest on
    if (ImGui::GetIO().KeyShift)
    {
        if (std::fabs(move.x) > std::fabs(move.y)) move.y = 0;
        else move.x = 0;
    }

    move = SnapMove(editor, items, drag.lo, drag.hi, drag.primary, move);

    for (const auto& moving : drag.moving)
    {
        if (Node* node = FindNode(editor.doc.level.nodes, moving.first))
        {
            SetNodeWorld(editor, *node, Mat4Translate({ move.x, move.y, 0 }) * moving.second);
        }
    }

    editor.doc.Changed();
}

void FinishBox(Editor& editor, const std::vector<CanvasItem>& items, Vec2 to)
{
    Vec2 from = editor.canvas.drag.from;
    Vec2 lo = { std::min(from.x, to.x), std::min(from.y, to.y) }, hi = { std::max(from.x, to.x), std::max(from.y, to.y) };

    for (const CanvasItem& item : items)
    {
        Vec2 corners[4];
        bool inside = !item.locked;
        ItemCorners(item, corners);

        for (const Vec2& c : corners) inside = inside && c.x >= lo.x && c.x <= hi.x && c.y >= lo.y && c.y <= hi.y;
        if (inside && !editor.doc.IsSelected(item.id)) editor.doc.Select(item.id, true);
    }
}

// is the mouse on it: inside its rectangle, or near a point item's position
bool ItemUnder(const Editor& editor, const CanvasItem& item, Vec2 at, ImVec2 mouse, ImVec2 origin, ImVec2 size)
{
    Vec3 local = TransformPoint(Mat4Inverse(item.world), { at.x, at.y, item.world.at(2, 3) });
    ImVec2 point = LevelToCanvas(editor, { item.world.at(0, 3), item.world.at(1, 3) }, origin, size);

    return item.sized ? local.x >= item.lo.x && local.x <= item.hi.x && local.y >= item.lo.y && local.y <= item.hi.y
                      : std::fabs(point.x - mouse.x) < 10 && std::fabs(point.y - mouse.y) < 10;
}

// what's under the mouse, topmost first: highest z, then the last in the tree, which is drawn last
std::vector<int> ItemsUnder(Editor& editor, const std::vector<CanvasItem>& items, Vec2 at, ImVec2 mouse, ImVec2 origin, ImVec2 size)
{
    std::vector<const CanvasItem*> under;

    for (const CanvasItem& item : items)
    {
        if (!item.locked && ItemUnder(editor, item, at, mouse, origin, size)) under.push_back(&item);
    }

    std::stable_sort(under.begin(), under.end(), [](const CanvasItem* a, const CanvasItem* b) { return a->world.at(2, 3) < b->world.at(2, 3); });
    std::vector<int> ids;
    for (auto it = under.rbegin(); it != under.rend(); ++it) ids.push_back((*it)->id);
    return ids;
}

// Alt right click: every unlocked item under the mouse, topmost first
void OpenUnderMenu(Editor& editor, const std::vector<CanvasItem>& items, Vec2 at, ImVec2 mouse, ImVec2 origin, ImVec2 size)
{
    editor.canvas.under = ItemsUnder(editor, items, at, mouse, origin, size);
    if (!editor.canvas.under.empty()) ImGui::OpenPopup("##canvasUnder");
}

void DrawUnderMenu(Editor& editor)
{
    if (!ImGui::BeginPopup("##canvasUnder")) return;

    ImGui::TextDisabled("Under the mouse");

    for (int id : editor.canvas.under)
    {
        const Node* node = FindNode(editor.doc.level.nodes, id);
        if (!node) continue;

        std::string label = node->name + "##" + std::to_string(id);
        if (ImGui::Selectable(label.c_str(), editor.doc.IsSelected(id))) editor.doc.Select(id, ImGui::GetIO().KeyCtrl);
    }

    ImGui::EndPopup();
}

// arrow keys: a pixel, Shift a grid step
void Nudge(Editor& editor)
{
    if (!editor.viewportHovered || ImGui::GetIO().WantTextInput || CanvasBusy(editor)) return;

    float step = ImGui::GetIO().KeyShift ? GridStep(editor) : 1.0f;
    Vec2 move = { 0, 0 };

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) move.x -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) move.x += step;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) move.y -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) move.y += step;

    if (move.x == 0 && move.y == 0) return;

    for (int id : editor.doc.TopSelection())
    {
        Node* node = FindNode(editor.doc.level.nodes, id);
        if (node && !node->locked) SetNodeWorld(editor, *node, Mat4Translate({ move.x, move.y, 0 }) * NodeWorld(editor.doc.level.nodes, id));
    }

    editor.doc.Changed();
}

// each Camera node's frame: what the game's window shows through it, turned like the node
void DrawCameraFrames(Editor& editor, ImVec2 origin, ImVec2 size)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();

    ForEachNode(editor.doc.level.nodes, [&](Node& node)
    {
        const Element* camera = nullptr;
        for (const Element& e : node.elements) if (e.type == ElementType::Camera) camera = &e;
        if (!camera || !node.visible) return;

        Mat4 world = NodeWorld(editor.doc.level.nodes, node.id);
        float w = kGameWidth / 2 / std::max(camera->zoom, 0.01f), h = kGameHeight / 2 / std::max(camera->zoom, 0.01f);
        Vec2 corners[4] = { { -w, -h }, { w, -h }, { w, h }, { -w, h } };
        ImVec2 points[4];
        Vec3 at = TransformPoint(world, { 0, 0, 0 });
        Vec3 right = Normalize(TransformDirection(world, { 1, 0, 0 }));
        Vec3 down = { -right.y, right.x, 0 };

        // its zoom and turn, not its scale: a camera's frame is the window
        for (int i = 0; i < 4; i++)
        {
            Vec2 p = { at.x + right.x * corners[i].x + down.x * corners[i].y, at.y + right.y * corners[i].x + down.y * corners[i].y };
            points[i] = LevelToCanvas(editor, p, origin, size);
        }

        draw->AddPolyline(points, 4, IM_COL32(180, 120, 255, 200), ImDrawFlags_Closed, 1.5f);
        draw->AddText(ImVec2(points[0].x + 4, points[0].y + 2), IM_COL32(180, 120, 255, 220), node.name.c_str());
    });
}

void DrawCanvas(Editor& editor, ImVec2 origin, ImVec2 size)
{
    ImGuiIO& io = ImGui::GetIO();
    CanvasDrag& drag = editor.canvas.drag;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 mouse = io.MousePos;
    Vec2 at = CanvasToLevel(editor, ImVec2(mouse.x - origin.x, mouse.y - origin.y), size);
    std::vector<CanvasItem> items = CanvasItems(editor);
    bool tools = UsesCanvas(editor);

    draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    DrawGuides(editor, origin, size);
    DrawCameraFrames(editor, origin, size);
    bool onRuler = DrawRulers(editor, origin, size);

    const CanvasItem* handles = tools ? HandleItem(editor, items) : nullptr;
    int hovered = handles && drag.kind == CanvasDragKind::None ? HandleUnder(editor, *handles, mouse, origin, size) : -1;

    if (tools && editor.viewportHovered && !onRuler && drag.kind == CanvasDragKind::None)
    {
        if (hovered >= 0) ImGui::SetMouseCursor(HandleCursor(hovered));

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            bool additive = io.KeyCtrl || io.KeyShift;
            drag.from = at;
            drag.fromScreen = mouse;
            drag.before = editor.doc.Snapshot();
            std::vector<int> under = ItemsUnder(editor, items, at, mouse, origin, size);

            if (hovered >= 0)
            {
                StartResize(editor, *handles, hovered);
            }
            else if (int hit = under.empty() ? 0 : under.front())
            {
                if (additive || !editor.doc.IsSelected(hit)) editor.doc.Select(hit, additive);
                drag.kind = editor.doc.IsSelected(hit) ? CanvasDragKind::Queued : CanvasDragKind::None;
            }
            else
            {
                if (!additive) editor.doc.ClearSelection();
                drag.kind = CanvasDragKind::Box;
            }
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && io.KeyAlt) OpenUnderMenu(editor, items, at, mouse, origin, size);
    }

    // a click becomes a move once the mouse has gone a few pixels
    if (drag.kind == CanvasDragKind::Queued && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        std::hypot(mouse.x - drag.fromScreen.x, mouse.y - drag.fromScreen.y) > 3.0f)
    {
        StartMove(editor, items);
    }

    if (drag.kind == CanvasDragKind::Move) UpdateMove(editor, items, at);
    if (drag.kind == CanvasDragKind::Resize) UpdateResize(editor, items, at);

    if (drag.kind == CanvasDragKind::Box)
    {
        ImVec2 a = drag.fromScreen;
        draw->AddRectFilled(ImVec2(std::min(a.x, mouse.x), std::min(a.y, mouse.y)), ImVec2(std::max(a.x, mouse.x), std::max(a.y, mouse.y)), IM_COL32(90, 160, 255, 40));
        draw->AddRect(ImVec2(std::min(a.x, mouse.x), std::min(a.y, mouse.y)), ImVec2(std::max(a.x, mouse.x), std::max(a.y, mouse.y)), IM_COL32(90, 160, 255, 200));
    }

    if (CanvasBusy(editor) && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && drag.kind != CanvasDragKind::Guide) CanvasCancel(editor);

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && drag.kind != CanvasDragKind::Guide)
    {
        if (drag.kind == CanvasDragKind::Box) FinishBox(editor, items, at);
        drag = CanvasDrag();
        editor.canvas.lines = CanvasSnapLines();
    }

    if (tools && handles && drag.kind != CanvasDragKind::Move) DrawHandles(editor, *handles, hovered, origin, size);

    DrawSnapLines(editor, origin, size);
    draw->PopClipRect();

    if (tools) Nudge(editor);
    DrawUnderMenu(editor);
}
