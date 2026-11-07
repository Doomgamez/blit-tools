// the node tree: click to select, ctrl+click for more, drag a node onto another to put it under it, or onto the
// empty space below to move it to the top. right click for the rest

#include "ui/Editor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

const char* const kNodePayload = "BLIT_NODE";

// what a node is, in grey after its name: its class, or what its elements make it
std::string NodeSummary(const Node& node, bool is2D)
{
    std::string summary = NodeClass(node, is2D);

    if (summary.empty())
    {
        summary = node.elements.empty() ? (node.children.empty() ? "" : "group") : "geometry";
    }

    return summary;
}

// a change to one node, by id: the tree may have moved by the time it runs
void DeferNodeChange(Editor& editor, int id, std::function<void(Node&)> change)
{
    Defer(editor, [&editor, id, change]()
    {
        if (Node* node = FindNode(editor.doc.level.nodes, id))
        {
            change(*node);
            editor.doc.Changed();
        }
    });
}

// everything here is deferred: it changes the tree the panel is still walking
void NodeContextMenu(Editor& editor, Node& node)
{
    Document& doc = editor.doc;
    int id = node.id;

    if (!doc.IsSelected(id)) doc.Select(id, false);

    if (ImGui::BeginMenu("Add child"))
    {
        DrawAddMenu(editor, id);
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) Defer(editor, [&doc]() { doc.DuplicateSelected(); });
    if (ImGui::MenuItem("Group", "Ctrl+G")) Defer(editor, [&doc]() { doc.GroupSelected(); });
    if (ImGui::MenuItem("Delete", "Del")) Defer(editor, [&doc]() { doc.DeleteSelected(); });
    ImGui::Separator();

    if (ImGui::MenuItem(node.visible ? "Hide in the editor" : "Show in the editor"))
        DeferNodeChange(editor, id, [](Node& n) { n.visible = !n.visible; });
    if (ImGui::MenuItem(node.locked ? "Unlock" : "Lock (can't be clicked)"))
        DeferNodeChange(editor, id, [](Node& n) { n.locked = !n.locked; });
    if (ImGui::MenuItem(node.exported ? "Leave out of the level" : "Put back in the level"))
        DeferNodeChange(editor, id, [](Node& n) { n.exported = !n.exported; });
    ImGui::Separator();
    if (ImGui::MenuItem("Focus", "F")) FocusSelection(editor);
}

// changes to the tree are deferred to the end of the frame, so walking it here is safe
void DrawNodeRow(Editor& editor, Node& node)
{
    Document& doc = editor.doc;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (doc.IsSelected(node.id)) flags |= ImGuiTreeNodeFlags_Selected;

    bool dim = !node.visible || !node.exported;
    if (dim) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

    std::string label = node.name + (node.locked ? "  [locked]" : "") + (!node.exported ? "  [not exported]" : "");
    bool open = ImGui::TreeNodeEx((void*)(intptr_t)node.id, flags, "%s", label.c_str());

    if (dim) ImGui::PopStyleColor();

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
    {
        doc.Select(node.id, ImGui::GetIO().KeyCtrl);
        editor.faceMode = false;
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        FocusSelection(editor);
    }

    for (const Element& e : node.elements)
    {
        GLuint picture = e.type == ElementType::Script ? GetScriptIcon(editor.scriptIcon, ImGui::GetTime()) : 0;

        if (picture)
        {
            float line = ImGui::GetTextLineHeight();
            ImGui::SameLine();
            ImGui::Image((ImTextureID)(intptr_t)picture, ImVec2(line, line));
        }
    }

    std::string summary = NodeSummary(node, doc.level.is2D);
    if (!summary.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", summary.c_str());
    }

    if (ImGui::BeginPopupContextItem())
    {
        NodeContextMenu(editor, node);
        ImGui::EndPopup();
    }

    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload(kNodePayload, &node.id, sizeof(int));
        ImGui::Text("%s", node.name.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodePayload))
        {
            int moved = *(const int*)payload->Data, parent = node.id;
            Defer(editor, [&doc, moved, parent]() { doc.Reparent(moved, parent, -1); });
        }

        ImGui::EndDragDropTarget();
    }

    if (open)
    {
        for (Node& child : node.children)
        {
            DrawNodeRow(editor, child);
        }

        ImGui::TreePop();
    }
}

void DrawHierarchy(Editor& editor)
{
    Document& doc = editor.doc;

    if (!ImGui::Begin("Hierarchy"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("+ Add")) ImGui::OpenPopup("##add");
    if (ImGui::BeginPopup("##add")) { DrawAddMenu(editor, 0); ImGui::EndPopup(); }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", doc.Title().c_str());
    ImGui::Separator();

    ImGui::BeginChild("##tree");

    // the world comes first, it's entity 0: clicking it shows the level's own settings
    if (ImGui::Selectable("World (level settings)", doc.selection.empty() && !editor.faceMode))
    {
        doc.ClearSelection();
        editor.faceMode = false;
    }

    for (Node& node : doc.level.nodes)
    {
        DrawNodeRow(editor, node);
    }

    // the space below the tree: a drop there moves the node to the top, a click selects nothing
    ImVec2 rest = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##below", ImVec2(std::max(rest.x, 1.0f), std::max(rest.y, 40.0f)));

    if (ImGui::IsItemClicked()) doc.ClearSelection();

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodePayload))
        {
            int moved = *(const int*)payload->Data;
            Defer(editor, [&doc, moved]() { doc.Reparent(moved, 0, -1); });
        }

        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem("##belowmenu"))
    {
        DrawAddMenu(editor, 0);
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::End();
}
