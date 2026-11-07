// script objects as a picture: over their nodes in the viewport, and the assets panel's classes, which drop onto
// the level. it's one picture for all of them, blit-tools/blit-editor/icons/script.png, painted by you

#include <imgui.h>

#include "core/Dialogs.h"
#include "core/Files.h"
#include "ui/Viewport.h"

const float kPictureSize = 40.0f;

// a level point to the viewport's pixels, false when it's behind the camera
bool ToScreen(const Mat4& viewProjection, Vec3 point, ImVec2 origin, ImVec2 size, ImVec2& out)
{
    float in[4] = { point.x, point.y, point.z, 1.0f }, clip[4] = { 0, 0, 0, 0 };

    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 4; c++) clip[r] += viewProjection.at(r, c) * in[c];
    }

    if (clip[3] <= 0.0001f)
    {
        return false;
    }

    out = ImVec2(origin.x + (clip[0] / clip[3] * 0.5f + 0.5f) * size.x, origin.y + (0.5f - clip[1] / clip[3] * 0.5f) * size.y);
    return true;
}

void DrawClassPictures(Editor& editor, const Mat4& view, const Mat4& projection, ImVec2 origin, ImVec2 size)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    Mat4 viewProjection = projection * view;
    double now = ImGui::GetTime();

    draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);

    ForEachNode(editor.doc.level.nodes, [&](const Node& node)
    {
        const Element* script = nullptr;

        for (const Element& e : node.elements)
        {
            if (e.type == ElementType::Script && !e.className.empty()) script = &e;
        }

        if (!script || !node.visible) return;

        GLuint picture = GetScriptIcon(editor.scriptIcon, now);
        ImVec2 at;

        if (!picture || !ToScreen(viewProjection, TransformPoint(NodeWorld(editor.doc.level.nodes, node.id), { 0, 0, 0 }), origin, size, at))
        {
            return;
        }

        float half = kPictureSize * 0.5f;
        bool selected = editor.doc.IsSelected(node.id);
        ImVec2 lo(at.x - half, at.y - half), hi(at.x + half, at.y + half);

        draw->AddImage((ImTextureID)(intptr_t)picture, lo, hi);
        if (selected) draw->AddRect(lo, hi, IM_COL32(255, 200, 40, 255), 3.0f, 0, 2.0f);
    });

    draw->PopClipRect();
}

void DropClass(Editor& editor, const std::string& className, const Ray& ray, ImVec2 mouse, ImVec2 size)
{
    PickHit hit = PickAt(editor, ray);
    Node* node = hit.node ? FindNode(editor.doc.level.nodes, hit.node) : nullptr;

    editor.status = "Class " + className;

    // onto a node: it gets that script, or its script changes to it
    if (node)
    {
        Element* script = FindElement(*node, ElementType::Script);

        if (!script)
        {
            node->elements.push_back(MakeElement(ElementType::Script));
            script = &node->elements.back();
        }

        script->className = className;
        editor.doc.Select(node->id, false);
        editor.doc.Changed();
        return;
    }

    // onto nothing: a new script object there
    Node made = MakeNodeOfKind(NodeKind::ScriptObject, editor.doc.level.is2D, GridStep(editor), "");
    Vec3 at = PointOnGrid(editor.camera, mouse.x, mouse.y, size.x, size.y);

    made.name = className;
    made.elements[0].className = className;
    made.position = editor.snap ? Snap(at, GridStep(editor)) : at;
    if (editor.doc.level.is2D) made.position.z = 0;
    editor.doc.AddNode(made, 0);
}

void DrawClassList(Editor& editor)
{
    float tile = 72.0f;
    int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (tile + ImGui::GetStyle().ItemSpacing.x + 8)));
    int column = 0;
    GLuint picture = GetScriptIcon(editor.scriptIcon, ImGui::GetTime());
    ImTextureID image = (ImTextureID)(intptr_t)(picture ? picture : editor.textures.white);

    ImGui::TextDisabled("The scripts in scripts/. Click one to open it, drag one onto a node to give it the script's "
                        "level object class, or into the level for a new script object.");

    for (const ScriptFile& script : editor.scripts)
    {
        std::string name = FileName(script.file);
        // what a drop gives the node: the file's first class that EXTENDS LevelObject
        const std::string* className = script.classes.empty() ? nullptr : &script.classes.front();

        if (column++ % columns) ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::PushID(script.file.c_str());
        // a click opens the file in whatever opens .bs files (VS Code), a drag gives a node its class
        if (ImGui::ImageButton("##script", image, ImVec2(tile, tile), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0.18f, 0.2f, 0.24f, 1.0f),
                               className ? ImVec4(1, 1, 1, 1) : ImVec4(1, 1, 1, 0.45f)))
        {
            std::string path = JoinPath(PathsOf(editor.project).scripts, script.file);
            OpenWithSystem(path);
            editor.status = "Opened " + script.file;
        }

        if (className && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(kClassPayload, className->data(), className->size());
            ImGui::Image(image, ImVec2(48, 48));
            ImGui::Text("%s (%s)", className->c_str(), name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::IsItemHovered())
        {
            std::string classes;
            for (const std::string& c : script.classes) classes += (classes.empty() ? "" : ", ") + c;
            ImGui::SetTooltip("%s\n%s", script.file.c_str(), classes.empty() ? "no CLASS ... EXTENDS LevelObject in it"
                                                                              : ("level objects: " + classes).c_str());
        }

        ImGui::PopID();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tile + 8);
        ImGui::TextUnformatted(name.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
    }

    if (editor.scripts.empty()) ImGui::TextDisabled("No scripts in scripts/ yet.");
}
