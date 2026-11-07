// keyboard shortcuts. single keys only act while the mouse is over the viewport and nothing is being typed,
// so W is "move" there and a letter in a text field

#include "ui/Editor.h"

#include <imgui.h>

#include "core/Files.h"
#include "geometry/Brush.h"

// a box around everything selected, false when nothing is
bool SelectionBounds(Editor& editor, Vec3& lo, Vec3& hi)
{
    bool any = false;
    SceneContext context = MakeSceneContext(editor);

    for (int id : editor.doc.selection)
    {
        const Node* node = FindNode(editor.doc.level.nodes, id);
        if (!node) continue;

        Mat4 world = NodeWorld(editor.doc.level.nodes, id);
        Vec3 origin = TransformPoint(world, { 0, 0, 0 });
        lo = any ? Min(lo, origin) : origin;
        hi = any ? Max(hi, origin) : origin;
        any = true;

        for (const Element& element : node->elements)
        {
            Vec3 a, b;
            if (!ElementPickBox(element, context, editor.sceneOptions, a, b)) continue;

            for (int i = 0; i < 8; i++)
            {
                Vec3 corner = TransformPoint(world, { i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z });
                lo = Min(lo, corner);
                hi = Max(hi, corner);
            }
        }
    }

    return any;
}

void FocusSelection(Editor& editor)
{
    Vec3 lo, hi;

    if (SelectionBounds(editor, lo, hi))
    {
        FrameBox(editor.camera, lo, hi);
    }
}

void StepGrid(Editor& editor, bool up)
{
    if (editor.doc.level.is2D)
    {
        editor.gridStep2D = up ? std::fmin(editor.gridStep2D * 2.0f, 256.0f) : std::fmax(editor.gridStep2D * 0.5f, 1.0f);
    }
    else
    {
        editor.gridStep = up ? std::fmin(editor.gridStep * 2.0f, 4.0f) : std::fmax(editor.gridStep * 0.5f, 0.0625f);
    }

    char text[64];
    snprintf(text, sizeof(text), "Grid %g %s", GridStep(editor), editor.doc.level.is2D ? "px" : "m");
    editor.status = text;
}

void HandleShortcuts(Editor& editor)
{
    ImGuiIO& io = ImGui::GetIO();
    Document& doc = editor.doc;
    bool ctrl = io.KeyCtrl, shift = io.KeyShift;
    bool typing = io.WantTextInput;
    bool flying = ImGui::IsMouseDown(ImGuiMouseButton_Right) && editor.viewportHovered;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) SaveCurrentLevel(editor);
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) { if (shift) PlayGame(editor); else PlayLevel(editor); }
    if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) CompileCurrentLevel(editor);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_B, false)) BuildProject(editor, false);
    if (shift && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) StopSteps(editor.runner);

    if (typing)
    {
        return;
    }

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) { if (shift) doc.Redo(); else doc.Undo(); }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) doc.Redo();

    bool any = !doc.selection.empty();

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false) && any) ImGui::SetClipboardText(doc.CopySelection().c_str());
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X, false) && any) { ImGui::SetClipboardText(doc.CopySelection().c_str()); doc.DeleteSelected(); }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) { const char* text = ImGui::GetClipboardText(); if (text) doc.Paste(text); }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && any) doc.DuplicateSelected();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_G, false) && any) doc.GroupSelected();
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && any) doc.DeleteSelected();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !shift) doc.ClearSelection();

    if (ctrl || flying || !editor.viewportHovered)
    {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) editor.tool = Tool::Select;
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) editor.tool = Tool::Move;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) editor.tool = Tool::Rotate;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) editor.tool = Tool::Scale;
    if (ImGui::IsKeyPressed(ImGuiKey_T, false)) editor.tool = Tool::Resize;
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) FocusSelection(editor);
    if (ImGui::IsKeyPressed(ImGuiKey_G, false)) editor.faceMode = !editor.faceMode;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) StepGrid(editor, false);
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) StepGrid(editor, true);
}
