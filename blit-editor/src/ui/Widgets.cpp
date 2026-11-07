#include "ui/Widgets.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include "ui/Editor.h"

bool Color255(const char* label, Vec3& color)
{
    float c[3] = { color.x / 255.0f, color.y / 255.0f, color.z / 255.0f };

    if (ImGui::ColorEdit3(label, c))
    {
        color = { std::round(c[0] * 255.0f), std::round(c[1] * 255.0f), std::round(c[2] * 255.0f) };
        return true;
    }

    return false;
}

bool TextWithChoices(const char* label, std::string& value, const std::vector<std::string>& choices, const char* hint)
{
    bool changed = false;
    float button = ImGui::GetFrameHeight();

    ImGui::PushID(label);
    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - button - ImGui::GetStyle().ItemInnerSpacing.x);
    changed |= ImGui::InputTextWithHint("##text", hint, &value);
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);

    if (ImGui::ArrowButton("##choices", ImGuiDir_Down)) ImGui::OpenPopup("##list");

    if (ImGui::BeginPopup("##list"))
    {
        if (choices.empty()) ImGui::TextDisabled("Nothing to choose from yet");

        for (const std::string& choice : choices)
        {
            if (ImGui::Selectable(choice.c_str(), choice == value))
            {
                value = choice;
                changed = true;
            }
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

bool PairsTable(const char* id, Pairs& pairs)
{
    bool changed = false;
    int remove = -1;

    ImGui::PushID(id);

    if (ImGui::BeginTable("##pairs", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());

        for (size_t i = 0; i < pairs.size(); i++)
        {
            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            changed |= ImGui::InputTextWithHint("##key", "key", &pairs[i].first);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            changed |= ImGui::InputTextWithHint("##value", "value", &pairs[i].second);
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x")) remove = (int)i;
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (remove >= 0)
    {
        pairs.erase(pairs.begin() + remove);
        changed = true;
    }

    if (ImGui::SmallButton("+ property"))
    {
        pairs.emplace_back("key" + std::to_string(pairs.size() + 1), "");
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

bool TextureField(Editor& editor, const char* label, std::string& texture)
{
    bool changed = false;
    float size = ImGui::GetFrameHeight() * 2.0f;
    const TextureInfo& info = GetTexture(editor.textures, texture);

    ImGui::PushID(label);

    if (ImGui::ImageButton("##thumb", (ImTextureID)(intptr_t)(texture.empty() ? editor.textures.white : info.id), ImVec2(size, size)))
    {
        ImGui::OpenPopup("##textures");
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::InputTextWithHint("##name", "texture name (no .png)", &texture);
    if (!texture.empty() && !info.found) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "no %s.png in the texture folders", texture.c_str());
    ImGui::EndGroup();

    if (ImGui::BeginPopup("##textures"))
    {
        int column = 0;

        for (const std::string& name : editor.textures.names)
        {
            const TextureInfo& option = GetTexture(editor.textures, name);

            if (column++ % 6) ImGui::SameLine();

            if (ImGui::ImageButton(name.c_str(), (ImTextureID)(intptr_t)option.id, ImVec2(56, 56)))
            {
                texture = name;
                changed = true;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%dx%d)", name.c_str(), option.width, option.height);
        }

        if (editor.textures.names.empty()) ImGui::TextDisabled("No textures: put PNGs in assets/textures");
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}

void HelpMark(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");

    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
