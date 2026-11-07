// face mode: the texture, offset, scale and rotation of the clicked faces

#include <imgui.h>

#include "ui/Inspector.h"
#include "ui/Widgets.h"

std::vector<Face*> SelectedFaces(Editor& editor)
{
    std::vector<Face*> out;

    for (const FaceRef& ref : editor.doc.faces)
    {
        Node* node = FindNode(editor.doc.level.nodes, ref.node);

        if (!node || ref.element < 0 || ref.element >= (int)node->elements.size()) continue;

        Element& element = node->elements[(size_t)ref.element];

        if (ref.brush < 0 || ref.brush >= (int)element.brushes.size()) continue;

        Brush& brush = element.brushes[(size_t)ref.brush];

        if (ref.face >= 0 && ref.face < (int)brush.faces.size()) out.push_back(&brush.faces[(size_t)ref.face]);
    }

    return out;
}

bool DrawFaces(Editor& editor)
{
    std::vector<Face*> faces = SelectedFaces(editor);
    bool changed = false;

    if (faces.empty())
    {
        ImGui::TextDisabled("Click faces in the viewport, ctrl+click for more");
        return false;
    }

    // the first face's values stand for all, a change goes to every one
    Face& first = *faces.front();
    std::string texture = first.texture;
    float offset[2] = { first.offset[0], first.offset[1] };
    float scale[2] = { first.scale[0], first.scale[1] };
    float rotation = first.rotation;

    ImGui::SeparatorText(faces.size() == 1 ? "Face" : "Faces");
    ImGui::Text("%d face%s", (int)faces.size(), faces.size() == 1 ? "" : "s");

    if (TextureField(editor, "Texture", texture))
    {
        for (Face* face : faces) face->texture = texture;
        changed = true;
    }

    if (ImGui::DragFloat2("Offset", offset, 0.5f, 0, 0, "%.1f px"))
    {
        for (Face* face : faces) { face->offset[0] = offset[0]; face->offset[1] = offset[1]; }
        changed = true;
    }

    if (ImGui::DragFloat2("Scale", scale, 0.01f, -64.0f, 64.0f, "%.3f"))
    {
        for (Face* face : faces) { face->scale[0] = scale[0]; face->scale[1] = scale[1]; }
        changed = true;
    }

    if (ImGui::DragFloat("Rotation", &rotation, 0.5f, -360.0f, 360.0f, "%.1f deg"))
    {
        for (Face* face : faces) face->rotation = rotation;
        changed = true;
    }

    if (ImGui::Button("Rotate 90"))
    {
        for (Face* face : faces) face->rotation = std::fmod(face->rotation + 90.0f, 360.0f);
        changed = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Flip U"))
    {
        for (Face* face : faces) face->scale[0] = -face->scale[0];
        changed = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Flip V"))
    {
        for (Face* face : faces) face->scale[1] = -face->scale[1];
        changed = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset"))
    {
        for (Face* face : faces)
        {
            face->offset[0] = face->offset[1] = 0.0f;
            face->scale[0] = face->scale[1] = 1.0f;
            face->rotation = 0.0f;
        }

        changed = true;
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Special textures: clip and nodraw collide unseen,\nskip and hint are left out");
    return changed;
}
