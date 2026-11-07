// the inspector's sprite element: picture, fit, flips, the sheet's frames and region, with a preview of the
// picture where a click picks the frame

#include <algorithm>
#include <cmath>

#include "imgui.h"

#include "render/Sprites.h"
#include "ui/Editor.h"
#include "ui/Inspector.h"
#include "ui/Widgets.h"

// the picture shrunk to the panel, the region in yellow, the frames' grid and the frame shown filled
bool DrawSheetPreview(Editor& editor, Element& e, const TextureInfo& picture)
{
    float pw = (float)std::max(picture.width, 1), ph = (float)std::max(picture.height, 1);
    float width = std::min(ImGui::GetContentRegionAvail().x, 260.0f);
    float scale = std::min(width / pw, 260.0f / ph);
    ImVec2 size(pw * scale, ph * scale);
    ImVec2 at = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    bool changed = false;

    ImGui::Image((ImTextureID)(intptr_t)(picture.id ? picture.id : editor.textures.white), size);
    bool clicked = ImGui::IsItemClicked();
    ImVec2 mouse = ImGui::GetMousePos();
    draw->AddRect(at, ImVec2(at.x + size.x, at.y + size.y), IM_COL32(120, 120, 130, 255));

    // the region the frames are cut from
    Element whole = e;
    whole.columns = whole.rows = 1;
    whole.frame = 0;
    float region[4];
    SpriteFrame(whole, pw, ph, region);
    ImVec2 r0(at.x + region[0] * scale, at.y + region[1] * scale);
    ImVec2 r1(r0.x + region[2] * scale, r0.y + region[3] * scale);

    int columns = std::max(1, e.columns), rows = std::max(1, e.rows);
    float cellW = region[2] * scale / columns, cellH = region[3] * scale / rows;

    for (int c = 1; c < columns; c++) draw->AddLine(ImVec2(r0.x + c * cellW, r0.y), ImVec2(r0.x + c * cellW, r1.y), IM_COL32(255, 255, 255, 90));
    for (int r = 1; r < rows; r++) draw->AddLine(ImVec2(r0.x, r0.y + r * cellH), ImVec2(r1.x, r0.y + r * cellH), IM_COL32(255, 255, 255, 90));
    draw->AddRect(r0, r1, IM_COL32(255, 200, 40, 255));

    float frame[4];
    SpriteFrame(e, pw, ph, frame);
    ImVec2 f0(at.x + frame[0] * scale, at.y + frame[1] * scale);
    draw->AddRectFilled(f0, ImVec2(f0.x + frame[2] * scale, f0.y + frame[3] * scale), IM_COL32(255, 200, 40, 70));

    // a click on a frame shows that one
    if (clicked && mouse.x >= r0.x && mouse.y >= r0.y && mouse.x < r1.x && mouse.y < r1.y)
    {
        int c = std::clamp((int)((mouse.x - r0.x) / cellW), 0, columns - 1);
        int r = std::clamp((int)((mouse.y - r0.y) / cellH), 0, rows - 1);
        changed = e.frame != r * columns + c;
        e.frame = r * columns + c;
    }

    return changed;
}

bool DrawSpriteElement(Editor& editor, Node& node, Element& e)
{
    const TextureInfo& picture = GetTexture(editor.textures, e.texture);
    const char* fits[] = { "Scaled", "Tiled", "Repeat" };
    bool changed = TextureField(editor, "Texture", e.texture);

    for (int i = 0; i < (int)SpriteFit::Count; i++)
    {
        if (i) ImGui::SameLine();
        if (ImGui::RadioButton(fits[i], (int)e.fit == i)) { e.fit = (SpriteFit)i; changed = true; }
    }

    ImGui::SameLine();
    HelpMark("Scaled: the frame once over the sprite.\nTiled: split into the tiles you give.\n"
             "Repeat: tiles at the frame's own size, a wider box gets more of them.");

    SpriteLayout layout = LayOutSprite(node, e, picture);

    if (e.fit == SpriteFit::Tiled) changed |= ImGui::DragFloat2("Tiles", &e.tiles.x, 0.05f, 0.1f, 1000.0f, "%.2g");
    if (e.fit == SpriteFit::Repeat) ImGui::TextDisabled("%.2g x %.2g tiles", layout.tiles.x, layout.tiles.y);

    changed |= ImGui::Checkbox("Flip H", &e.flipX);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Flip V", &e.flipY);

    ImGui::TextDisabled(FindElement(node, ElementType::Brush) ? "Covers the node's box, scale the node to resize it"
                                                               : "The frame's size, scale the node to resize it");
    changed |= Color255("Tint", e.color);

    if (ImGui::TreeNodeEx("Sheet", e.columns * e.rows > 1 || e.useRegion ? ImGuiTreeNodeFlags_DefaultOpen : 0))
    {
        int cells[2] = { e.columns, e.rows };

        if (ImGui::DragInt2("Columns, rows", cells, 0.05f, 1, 256))
        {
            e.columns = std::max(1, cells[0]);
            e.rows = std::max(1, cells[1]);
            changed = true;
        }

        changed |= ImGui::SliderInt("Frame", &e.frame, 0, std::max(1, e.columns) * std::max(1, e.rows) - 1);
        changed |= ImGui::Checkbox("Region", &e.useRegion);
        ImGui::SameLine();
        HelpMark("Only this part of the picture, in pixels: x, y, width, height. The frames are cut from it.");

        if (e.useRegion)
        {
            if (e.region[2] <= 0 || e.region[3] <= 0)
            {
                e.region[0] = e.region[1] = 0;
                e.region[2] = (float)picture.width;
                e.region[3] = (float)picture.height;
            }

            changed |= ImGui::DragFloat4("Pixels", e.region, 0.5f, 0.0f, 16384.0f, "%.0f");
        }

        changed |= DrawSheetPreview(editor, e, picture);
        ImGui::TextDisabled("%s: %dx%d, frame %.0fx%.0f", e.texture.empty() ? "no picture" : e.texture.c_str(),
                            picture.width, picture.height, layout.frame[2], layout.frame[3]);
        ImGui::TreePop();
    }

    return changed;
}
