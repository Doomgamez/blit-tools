// Text elements in the viewport, drawn over the level with the editor's font where the game puts them: in the level
// at their node (facing the camera in 3D), or on the screen with their node's x y as pixels from the window's top left

#include <cfloat>
#include <cmath>

#include "imgui.h"

#include "ui/Canvas.h"
#include "ui/Editor.h"
#include "ui/Viewport.h"

// where its anchor is on the viewport and how tall its letters are there, false when it can't be seen
bool TextPlace(Editor& editor, const Element& text, const Mat4& world, const Mat4& viewProjection, ImVec2 origin,
               ImVec2 size, ImVec2& at, float& height)
{
    Vec3 p = TransformPoint(world, { 0, 0, 0 });
    const Camera& camera = editor.camera;

    // a HUD text over a 3D view: the game's window squeezed into the viewport
    if (text.screen && camera.kind != ViewKind::TwoD)
    {
        float scale = size.x / kGameWidth;
        at = ImVec2(origin.x + p.x * scale, origin.y + p.y * scale);
        height = text.textSize * scale;
        return true;
    }

    if (!ToScreen(viewProjection, p, origin, size, at))
    {
        return false;
    }

    if (IsOrthographic(camera))
    {
        height = text.textSize / std::max(camera.zoom, 1e-5f);
        return true;
    }

    float distance = Length(p - camera.position);
    height = text.textSize * size.y / (2.0f * distance * std::tan(camera.fov * 0.5f * 3.14159265f / 180.0f));
    return distance > 0.05f;
}

void DrawTextNodes(Editor& editor, const Mat4& view, const Mat4& projection, ImVec2 origin, ImVec2 size)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    Mat4 viewProjection = projection * view;

    draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);

    ForEachNode(editor.doc.level.nodes, [&](Node& node)
    {
        if (!node.visible) return;

        for (const Element& text : node.elements)
        {
            ImVec2 at;
            float height;

            if (text.type != ElementType::Text || text.text.empty() ||
                !TextPlace(editor, text, NodeWorld(editor.doc.level.nodes, node.id), viewProjection, origin, size, at, height) ||
                height < 2.0f || height > 4000.0f)
            {
                continue;
            }

            // lined up like the game: by its alignment across, its top (screen) or middle (level) on the node
            ImVec2 extent = font->CalcTextSizeA(height, FLT_MAX, 0.0f, text.text.c_str());
            at.x -= text.align == 0 ? 0.0f : text.align == 2 ? extent.x : extent.x / 2;
            at.y -= text.screen ? 0.0f : extent.y / 2;
            ImU32 colour = IM_COL32((int)text.color.x, (int)text.color.y, (int)text.color.z, 255);
            draw->AddText(font, height, at, colour, text.text.c_str());
        }
    });

    draw->PopClipRect();
}
