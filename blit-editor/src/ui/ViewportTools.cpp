// picking, the transform handles and drops from the assets panel

#include <algorithm>

#include <imgui.h>
#include <ImGuizmo.h>

#include "core/Files.h"
#include "geometry/Brush.h"
#include "ui/Viewport.h"

const char* const kTexturePayload = "BLIT_TEXTURE";
const char* const kModelPayload = "BLIT_MODEL";
const char* const kClassPayload = "BLIT_CLASS";

void PickNode(Editor& editor, const Node& node, const Mat4& parent, const Ray& ray, PickHit& best)
{
    if (!node.visible)
    {
        return;
    }

    Mat4 world = parent * NodeLocal(node);
    Mat4 inverse = Mat4Inverse(world);
    Ray local = TransformRay(inverse, ray);
    SceneContext context = MakeSceneContext(editor);

    for (size_t e = 0; !node.locked && e < node.elements.size(); e++)
    {
        const Element& element = node.elements[e];

        if (element.type == ElementType::Brush)
        {
            for (size_t b = 0; b < element.brushes.size(); b++)
            {
                float t;
                int face;

                if (!RayBrush(local, element.brushes[b], t, face)) continue;

                // distances compared in level space: a scaled node's t isn't metres
                float distance = Length(TransformPoint(world, local.origin + local.direction * t) - ray.origin);

                if (distance < best.distance)
                {
                    best.distance = distance;
                    best.node = node.id;
                    best.face = { node.id, (int)e, (int)b, face };
                }
            }

            continue;
        }

        Vec3 lo, hi;
        float t;

        if (!ElementPickBox(node, element, context, editor.sceneOptions, lo, hi)) continue;

        // icons keep their size: their box is around the origin in level space
        bool sized = element.type == ElementType::Model || element.type == ElementType::Sprite;
        Mat4 frame = sized ? world : Mat4Translate(TransformPoint(world, { 0, 0, 0 }));
        Ray inFrame = sized ? local : TransformRay(Mat4Inverse(frame), ray);

        if (RayBox(inFrame, lo, hi, t))
        {
            float distance = Length(TransformPoint(frame, inFrame.origin + inFrame.direction * t) - ray.origin);

            if (distance < best.distance)
            {
                best.distance = distance;
                best.node = node.id;
                best.face = FaceRef();
            }
        }
    }

    for (const Node& child : node.children)
    {
        PickNode(editor, child, world, ray, best);
    }
}

PickHit PickAt(Editor& editor, const Ray& ray)
{
    PickHit best;

    for (const Node& node : editor.doc.level.nodes)
    {
        PickNode(editor, node, Mat4(), ray, best);
    }

    return best;
}

void ClickSelect(Editor& editor, const Ray& ray, bool additive)
{
    Document& doc = editor.doc;
    PickHit hit = PickAt(editor, ray);

    if (editor.faceMode)
    {
        if (!additive) doc.faces.clear();

        if (hit.face.node)
        {
            auto found = std::find(doc.faces.begin(), doc.faces.end(), hit.face);
            if (found != doc.faces.end()) doc.faces.erase(found);
            else doc.faces.push_back(hit.face);
        }

        return;
    }

    if (!hit.node)
    {
        if (!additive) doc.ClearSelection();
        return;
    }

    doc.Select(hit.node, additive);
}

void AddFaceOutlines(Editor& editor, std::vector<LineVertex>& lines)
{
    const float color[4] = { 1.0f, 0.3f, 0.8f, 1.0f };

    if (!editor.faceMode) return;

    for (const FaceRef& ref : editor.doc.faces)
    {
        const Node* node = FindNode(editor.doc.level.nodes, ref.node);

        if (!node || ref.element >= (int)node->elements.size() || ref.brush >= (int)node->elements[(size_t)ref.element].brushes.size())
        {
            continue;
        }

        Mat4 world = NodeWorld(editor.doc.level.nodes, ref.node);

        for (const FacePolygon& polygon : BrushPolygons(node->elements[(size_t)ref.element].brushes[(size_t)ref.brush]))
        {
            if (polygon.face != ref.face) continue;

            for (size_t i = 0; i < polygon.points.size(); i++)
            {
                AddLine(lines, TransformPoint(world, polygon.points[i]),
                        TransformPoint(world, polygon.points[(i + 1) % polygon.points.size()]), color);
            }
        }
    }
}

bool DrawResizeGizmo(Editor& editor, Node& node, const Mat4& view, const Mat4& projection)
{
    ResizeDrag& drag = editor.resize;
    Vec3 lo, hi;
    bool any = false;

    for (const Element& element : node.elements)
    {
        Vec3 a, b;
        if (element.type == ElementType::Brush && ElementBounds(element, a, b))
        {
            lo = any ? Min(lo, a) : a;
            hi = any ? Max(hi, b) : b;
            any = true;
        }
    }

    if (!any) return false;

    // a drag works on the brushes as they were when it started, so snapping doesn't creep
    if (!drag.active || drag.node != node.id)
    {
        drag.node = node.id;
        drag.matrix = NodeWorld(editor.doc.level.nodes, node.id);
        float bounds[6] = { lo.x, lo.y, lo.z, hi.x, hi.y, hi.z };
        std::copy(bounds, bounds + 6, drag.bounds);
        drag.original = node.elements;
    }

    float step = GridStep(editor);
    float snap[3] = { step, step, step };
    Mat4 before = NodeWorld(editor.doc.level.nodes, node.id);

    if (ImGuizmo::Manipulate(view.m, projection.m, ImGuizmo::BOUNDS, ImGuizmo::LOCAL, drag.matrix.m, nullptr, nullptr,
                             drag.bounds, editor.snap ? snap : nullptr))
    {
        // what the handles did, in the node's own space, goes into the planes. the node stays put
        Mat4 change = Mat4Inverse(before) * drag.matrix;
        node.elements = drag.original;

        for (Element& element : node.elements)
        {
            for (Brush& brush : element.brushes) TransformBrush(brush, change);
        }

        editor.doc.Changed();
    }

    drag.active = ImGuizmo::IsUsing();
    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

bool DrawGizmo(Editor& editor, const Mat4& view, const Mat4& projection, ImVec2 origin, ImVec2 size)
{
    Document& doc = editor.doc;
    Node* primary = doc.Primary();
    bool is2D = doc.level.is2D;

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);
    ImGuizmo::SetOrthographic(IsOrthographic(editor.camera));

    if (!primary || editor.faceMode || editor.tool == Tool::Select)
    {
        editor.resize.active = false;
        return false;
    }

    if (editor.tool == Tool::Resize)
    {
        return HasElement(*primary, ElementType::Brush) && DrawResizeGizmo(editor, *primary, view, projection);
    }

    editor.resize.active = false;

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    float step = GridStep(editor);
    float snap[3] = { step, step, step };

    if (editor.tool == Tool::Move) operation = is2D ? (ImGuizmo::OPERATION)(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y) : ImGuizmo::TRANSLATE;
    if (editor.tool == Tool::Rotate) { operation = is2D ? ImGuizmo::ROTATE_Z : ImGuizmo::ROTATE; snap[0] = snap[1] = snap[2] = editor.rotateStep; }
    if (editor.tool == Tool::Scale) { operation = is2D ? (ImGuizmo::OPERATION)(ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y) : ImGuizmo::SCALE; snap[0] = snap[1] = snap[2] = 0.1f; }

    Mat4 world = NodeWorld(doc.level.nodes, primary->id);
    Mat4 delta;
    ImGuizmo::MODE mode = editor.worldSpace && editor.tool != Tool::Scale ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    if (ImGuizmo::Manipulate(view.m, projection.m, operation, mode, world.m, delta.m, editor.snap ? snap : nullptr))
    {
        auto setWorld = [&](Node& node, const Mat4& newWorld)
        {
            int parent = FindParentId(doc.level.nodes, node.id);
            Mat4 parentWorld = parent > 0 ? NodeWorld(doc.level.nodes, parent) : Mat4();
            Mat4Decompose(Mat4Inverse(parentWorld) * newWorld, node.position, node.rotation, node.scale);
        };

        if (editor.tool == Tool::Scale)
        {
            setWorld(*primary, world); // scaling is the primary's alone
        }
        else
        {
            // the same move for everything selected, by the delta in level space
            for (int id : doc.TopSelection())
            {
                if (Node* node = FindNode(doc.level.nodes, id))
                {
                    setWorld(*node, delta * NodeWorld(doc.level.nodes, id));
                }
            }
        }

        doc.Changed();
    }

    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

void HandleViewportDrops(Editor& editor, const Ray& ray, ImVec2 mouse, ImVec2 size)
{
    if (!ImGui::BeginDragDropTarget())
    {
        return;
    }

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kTexturePayload))
    {
        std::string texture((const char*)payload->Data, (size_t)payload->DataSize);
        PickHit hit = PickAt(editor, ray);
        Node* node = hit.node ? FindNode(editor.doc.level.nodes, hit.node) : nullptr;

        // in 2D a texture dropped on a box is its sprite, which covers it
        if (node && editor.doc.level.is2D && !editor.faceMode)
        {
            Element* sprite = FindElement(*node, ElementType::Sprite);

            if (!sprite)
            {
                node->elements.push_back(MakeElement(ElementType::Sprite));
                sprite = &node->elements.back();
            }

            sprite->texture = texture;
            editor.doc.Changed();
        }
        // a texture dropped on a face textures that face in face mode, the whole brush otherwise
        else if (node && hit.face.node)
        {
            Element& element = node->elements[(size_t)hit.face.element];
            Brush& brush = element.brushes[(size_t)hit.face.brush];

            if (editor.faceMode) brush.faces[(size_t)hit.face.face].texture = texture;
            else SetBrushTexture(brush, texture);

            editor.doc.Changed();
        }
        else if (node && FindElement(*node, ElementType::Sprite))
        {
            FindElement(*node, ElementType::Sprite)->texture = texture;
            editor.doc.Changed();
        }

        editor.currentTexture = texture;
    }

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kClassPayload))
    {
        DropClass(editor, std::string((const char*)payload->Data, (size_t)payload->DataSize), ray, mouse, size);
    }

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kModelPayload))
    {
        std::string file((const char*)payload->Data, (size_t)payload->DataSize);
        Node node = MakeNodeOfKind(NodeKind::Model, editor.doc.level.is2D, GridStep(editor), "");
        Vec3 at = PointOnGrid(editor.camera, mouse.x, mouse.y, size.x, size.y);

        node.name = FileStem(file);
        node.elements[0].file = file;
        node.position = editor.snap ? Snap(at, GridStep(editor)) : at;
        editor.doc.AddNode(node, 0);
    }

    ImGui::EndDragDropTarget();
}
