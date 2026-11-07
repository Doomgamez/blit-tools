// the selected node (its transform and its elements), the selected faces in face mode, or the level's own settings

#include "ui/Editor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include "ui/Inspector.h"
#include "ui/Widgets.h"

const std::vector<std::string> kKnownClasses = {
    "info_player_start", "info_target", "info_null", "info_camera", "object_script", "light_point", "prop_model",
    "ambient_sound", "func_detail", "func_collision", "func_door", "func_move", "trigger_script", "solid_2d", "sprite_2d",
};

bool DrawLevelSettings(Editor& editor)
{
    Level& level = editor.doc.level;
    bool changed = false;

    ImGui::SeparatorText("Level");
    ImGui::Text("%s level, %s", level.is2D ? "2D" : "3D", level.is2D ? "pixels, Y down" : "metres, Y up");

    if (!level.is2D)
    {
        changed |= ImGui::DragFloat("Texels per metre", &level.texelsPerMetre, 1.0f, 1.0f, 1024.0f, "%.0f");
        HelpMark("How many pixels of a texture cover a metre at texture scale 1. 32 is what the old TrenchBroom maps used.");
    }

    ImGui::SeparatorText("World properties");
    ImGui::TextDisabled("Entity 0 (worldspawn): GetEntityProperty level 0 \"music\"");
    changed |= PairsTable("world", level.world);

    ImGui::SeparatorText("Counts");
    int nodes = 0, brushes = 0;
    ForEachNode(level.nodes, [&](const Node& node)
    {
        nodes++;
        for (const Element& e : node.elements) brushes += (int)e.brushes.size();
    });
    ImGui::Text("%d nodes, %d brushes", nodes, brushes);
    return changed;
}

bool DrawTransform(Editor& editor, Node& node)
{
    bool changed = false;
    float speed = editor.doc.level.is2D ? 1.0f : 0.05f;

    ImGui::SeparatorText("Transform");
    changed |= ImGui::DragFloat3("Position", &node.position.x, speed, 0, 0, "%.3f");
    if (editor.doc.level.is2D) HelpMark("In 2D, z is what's on top: a higher z is drawn over a lower one, nodes with the "
                                        "same z follow the hierarchy. A child's z adds to its parent's, like in 3D.");

    if (editor.doc.level.is2D)
    {
        changed |= ImGui::DragFloat("Rotation", &node.rotation.z, 0.5f, -360, 360, "%.1f deg");
        changed |= ImGui::DragFloat2("Scale", &node.scale.x, 0.01f, 0, 0, "%.3f");
    }
    else
    {
        changed |= ImGui::DragFloat3("Rotation", &node.rotation.x, 0.5f, -360, 360, "%.1f");
        HelpMark("Pitch, yaw, roll in degrees, the order the engine's SetNodeRotationEuler uses. Yaw 0 faces +X.");
        changed |= ImGui::DragFloat3("Scale", &node.scale.x, 0.01f, 0, 0, "%.3f");
    }

    if (ImGui::SmallButton("Reset"))
    {
        node.position = { 0, 0, 0 };
        node.rotation = { 0, 0, 0 };
        node.scale = { 1, 1, 1 };
        changed = true;
    }

    return changed;
}

bool DrawElements(Editor& editor, Node& node)
{
    bool changed = false;
    int remove = -1, moveUp = -1;

    ImGui::SeparatorText("Elements");

    for (size_t i = 0; i < node.elements.size(); i++)
    {
        Element& element = node.elements[i];
        ImGui::PushID((int)i);

        bool open = ImGui::CollapsingHeader(ElementTitle(element.type), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
        if (i > 0 && ImGui::SmallButton("^")) moveUp = (int)i;
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) remove = (int)i;

        if (open)
        {
            ImGui::Indent(6);
            changed |= DrawElement(editor, node, element);
            ImGui::Unindent(6);
        }

        ImGui::PopID();
    }

    if (remove >= 0) { node.elements.erase(node.elements.begin() + remove); changed = true; }
    if (moveUp > 0) { std::swap(node.elements[(size_t)moveUp], node.elements[(size_t)moveUp - 1]); changed = true; }

    if (ImGui::Button("+ Add element", ImVec2(-1, 0))) ImGui::OpenPopup("##addelement");

    if (ImGui::BeginPopup("##addelement"))
    {
        for (int t = 0; t < (int)ElementType::Count; t++)
        {
            ElementType type = (ElementType)t;
            bool fits = !(type == ElementType::Sprite && !editor.doc.level.is2D) && !(type == ElementType::Model && editor.doc.level.is2D);

            if (fits && ImGui::Selectable(ElementTitle(type)))
            {
                node.elements.push_back(NewElementFor(editor, type));
                changed = true;
            }
        }

        ImGui::EndPopup();
    }

    return changed;
}

bool DrawNode(Editor& editor, Node& node)
{
    bool changed = false;
    std::string compiled = NodeClass(node, editor.doc.level.is2D);

    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::InputText("##name", &node.name);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("The entity's name, what FindLevelEntity and FindLevelObject look for");

    changed |= TextWithChoices("Class", node.className, kKnownClasses, compiled.empty() ? "(from the elements)" : compiled.c_str());
    HelpMark("Leave it empty and the elements decide (a light makes a light_point). Any name works: scripts read it with "
             "GetEntityClass, and a new gameplay class needs no engine change.");
    ImGui::TextDisabled("Compiles as: %s", compiled.empty() ? "plain geometry / group (not an entity)" : compiled.c_str());

    changed |= ImGui::Checkbox("Visible", &node.visible);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Locked", &node.locked);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Export", &node.exported);
    HelpMark("Visible and Locked are only for the editor. Export off leaves the node and its children out of the "
             "compiled level, like a layer that's hidden from export.");

    changed |= DrawTransform(editor, node);
    changed |= DrawElements(editor, node);
    return changed;
}

void DrawInspector(Editor& editor)
{
    Document& doc = editor.doc;

    if (!ImGui::Begin("Inspector"))
    {
        ImGui::End();
        return;
    }

    bool changed = false;
    Node* primary = doc.Primary();

    if (editor.faceMode && !doc.faces.empty())
    {
        changed |= DrawFaces(editor);
    }
    else if (!primary)
    {
        changed |= DrawLevelSettings(editor);
    }
    else
    {
        if (doc.selection.size() > 1)
        {
            ImGui::TextDisabled("%d nodes selected, this is the last one", (int)doc.selection.size());
        }

        changed |= DrawNode(editor, *primary);
    }

    if (changed)
    {
        doc.Changed();
    }

    ImGui::End();
}
