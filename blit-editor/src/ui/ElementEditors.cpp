// one editor per element type

#include <algorithm>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "core/Files.h"
#include "geometry/Brush.h"
#include "ui/Inspector.h"
#include "ui/Widgets.h"

Element NewElementFor(Editor& editor, ElementType type)
{
    Element element = MakeElement(type);
    bool is2D = editor.doc.level.is2D;
    float s = GridStep(editor) * 4.0f;

    if (type == ElementType::Brush)
    {
        Vec3 half = is2D ? Vec3(s, s * 0.5f, 16) : Vec3(s * 0.5f, s * 0.5f, s * 0.5f);
        element.brushes.push_back(MakeBox(-half, half, is2D ? "" : editor.currentTexture));
    }

    if (type == ElementType::Sprite)
    {
        element.texture = editor.currentTexture;
        element.size = { s, s };
    }

    if (type == ElementType::Light && is2D) element.range = 200.0f;
    if (type == ElementType::Model && !editor.modelFiles.empty()) element.file = editor.modelFiles.front();
    return element;
}

bool DrawBrushElement(Editor& editor, Node& node, Element& element)
{
    bool changed = false;
    int role = (int)element.role;
    const char* roles[(int)BrushRole::Count];

    for (int r = 0; r < (int)BrushRole::Count; r++) roles[r] = RoleTitle((BrushRole)r);

    if (ImGui::Combo("Role", &role, roles, (int)BrushRole::Count))
    {
        element.role = (BrushRole)role;
        changed = true;
    }

    HelpMark("Solid: drawn and collides. Detail: drawn, nothing collides (railings). Collision: an invisible wall. "
             "Trigger: neither, scripts ask whether something is inside (IsPointInEntity).");

    Vec3 lo, hi;
    if (!ElementBounds(element, lo, hi))
    {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "These brushes enclose nothing");
        return changed;
    }

    // the brushes' box, editing it stretches every brush to fit
    Vec3 size = hi - lo, center = (lo + hi) * 0.5f;
    Vec3 newSize = size, newCenter = center;
    float speed = editor.doc.level.is2D ? 1.0f : 0.05f;

    ImGui::Text("%d brush%s", (int)element.brushes.size(), element.brushes.size() == 1 ? "" : "es");
    bool resized = ImGui::DragFloat3("Size", &newSize.x, speed, 0.001f, 100000.0f, "%.3f");
    resized |= ImGui::DragFloat3("Centre", &newCenter.x, speed, 0, 0, "%.3f");

    if (resized)
    {
        newSize = Max(newSize, { 0.001f, 0.001f, 0.001f });

        for (Brush& brush : element.brushes)
        {
            FitBrush(brush, lo, hi, newCenter - newSize * 0.5f, newCenter + newSize * 0.5f);
        }

        changed = true;
    }

    if (!editor.doc.level.is2D)
    {
        std::string texture = element.brushes.empty() || element.brushes[0].faces.empty() ? "" : element.brushes[0].faces[0].texture;

        if (TextureField(editor, "Texture (every face)", texture))
        {
            for (Brush& brush : element.brushes) SetBrushTexture(brush, texture);
            changed = true;
        }
    }

    if (ImGui::Button("Hollow"))
    {
        // the box into six walls one grid step thick, a room to stand in
        element.brushes = MakeHollow(lo, hi, GridStep(editor), element.brushes[0].faces.empty() ? "" : element.brushes[0].faces[0].texture);
        changed = true;
    }

    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Replace the brushes with six walls, one grid step thick");
    ImGui::SameLine();

    if (ImGui::Button("Apply node scale"))
    {
        // bakes the node's scale into the planes, so textures keep their size
        for (Brush& brush : element.brushes) TransformBrush(brush, Mat4Scale(node.scale));
        for (Element& other : node.elements)
        {
            if (&other != &element && other.type == ElementType::Brush)
            {
                for (Brush& brush : other.brushes) TransformBrush(brush, Mat4Scale(node.scale));
            }
        }

        node.scale = { 1, 1, 1 };
        changed = true;
    }

    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Make the brushes this size and the node's scale 1, so textures aren't stretched");
    ImGui::SameLine();

    if (ImGui::Button("Centre origin"))
    {
        // the node to the middle of its brushes, they stay where they are in the level
        Vec3 shift = TransformDirection(NodeLocal(node), center);
        for (Element& any : node.elements)
        {
            for (Brush& brush : any.brushes) TransformBrush(brush, Mat4Translate(-center));
        }

        node.position = node.position + shift;
        changed = true;
    }

    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the node's origin (what doors turn around) to the middle of its brushes");
    return changed;
}

bool DrawElement(Editor& editor, Node& node, Element& e)
{
    bool changed = false;

    switch (e.type)
    {
    case ElementType::Brush:
        return DrawBrushElement(editor, node, e);

    case ElementType::Light:
        changed |= Color255("Colour", e.color);
        changed |= ImGui::DragFloat("Range", &e.range, 0.05f, 0.0f, 10000.0f, editor.doc.level.is2D ? "%.0f px" : "%.2f m");
        changed |= ImGui::DragFloat("Intensity", &e.intensity, 0.01f, 0.0f, 100.0f, "%.2f");
        break;

    case ElementType::Model:
        changed |= TextWithChoices("File", e.file, editor.modelFiles, "assets/models/house.glb");
        changed |= Color255("Tint", e.color);
        ImGui::TextDisabled("Scale it with the node's scale");
        break;

    case ElementType::Spawn:
        ImGui::TextWrapped("Where the player starts: FindLevelEntity level \"info_player_start\" 0. It faces along the node's +X.");
        break;

    case ElementType::Marker:
        ImGui::TextWrapped("A named point for scripts: camera spots, patrol points. FindLevelEntity level \"%s\" 0.", node.name.c_str());
        break;

    case ElementType::Camera:
        changed |= ImGui::DragFloat("Field of view", &e.fov, 0.5f, 10.0f, 170.0f, "%.0f deg");
        break;

    case ElementType::Sound:
        changed |= TextWithChoices("File", e.file, editor.soundFiles, "assets/sounds/hum.wav");
        changed |= ImGui::SliderFloat("Volume", &e.volume, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Radius", &e.radius, 0.1f, 0.0f, 10000.0f);
        changed |= ImGui::Checkbox("Loop", &e.loop);
        break;

    case ElementType::Sprite:
        changed |= TextureField(editor, "Texture", e.texture);
        changed |= ImGui::DragFloat2("Size", &e.size.x, 1.0f, 1.0f, 100000.0f, "%.0f px");
        changed |= ImGui::DragInt("Layer", &e.layer, 0.1f, -100, 100);
        changed |= ImGui::Checkbox("Tiled", &e.tiled);
        changed |= Color255("Tint", e.color);
        break;

    case ElementType::Mover:
        if (ImGui::RadioButton("Door (func_door)", e.door)) { e.door = true; changed = true; }
        ImGui::SameLine();
        if (ImGui::RadioButton("Mover (func_move)", !e.door)) { e.door = false; changed = true; }
        changed |= ImGui::DragFloat("Speed", &e.speed, 0.5f, 0.0f, 10000.0f);
        ImGui::TextDisabled("Its own node, a script moves it: GetEntityNode");
        break;

    case ElementType::Script:
        if (GLuint picture = GetClassIcon(editor.classIcons, e.className, ImGui::GetTime()))
        {
            ImGui::Image((ImTextureID)(intptr_t)picture, ImVec2(48, 48));
            ImGui::SameLine();
        }

        changed |= TextWithChoices("Class", e.className, editor.classes, "Door");
        HelpMark("A BlitScript class that EXTENDS LevelObject (level.bs). SpawnLevelObjects makes one for this node, "
                 "its Spawn, Update, Draw and Use run with the game. The list shows the classes in scripts/.");
        if (!e.className.empty() && std::find(editor.classes.begin(), editor.classes.end(), e.className) == editor.classes.end())
        {
            ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "No CLASS %s EXTENDS LevelObject in scripts/ yet", e.className.c_str());
        }
        break;

    case ElementType::Properties:
        ImGui::TextDisabled("GetEntityProperty and the typed getters read these");
        changed |= PairsTable("values", e.values);
        break;

    default:
        break;
    }

    return changed;
}
