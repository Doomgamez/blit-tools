// the project's textures, models, levels and level object classes. drag a texture or model into the viewport

#include <imgui.h>

#include "core/Dialogs.h"
#include "core/Files.h"
#include "project/Build.h"
#include "ui/Viewport.h"
#include "ui/Widgets.h"

void DrawTextureGrid(Editor& editor)
{
    float tile = 72.0f;
    int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (tile + ImGui::GetStyle().ItemSpacing.x + 8)));
    int column = 0;

    ImGui::TextDisabled("Click: the texture new brushes and Apply use. Drag onto a brush or face to paint it.");

    for (const std::string& name : editor.textures.names)
    {
        const TextureInfo& info = GetTexture(editor.textures, name);
        bool current = editor.currentTexture == name;

        if (column++ % columns) ImGui::SameLine();

        ImGui::BeginGroup();
        if (current) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::ImageButton(name.c_str(), (ImTextureID)(intptr_t)info.id, ImVec2(tile, tile))) editor.currentTexture = name;
        if (current) ImGui::PopStyleColor();

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(kTexturePayload, name.data(), name.size());
            ImGui::Image((ImTextureID)(intptr_t)info.id, ImVec2(48, 48));
            ImGui::Text("%s", name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s  %dx%d", name.c_str(), info.width, info.height);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tile + 8);
        ImGui::TextUnformatted(name.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
    }

    if (editor.textures.names.empty()) ImGui::TextDisabled("No textures yet: put PNGs in assets/textures.");
}

void DrawModelList(Editor& editor)
{
    ImGui::TextDisabled("Drag a model into the viewport to place it as a prop_model.");

    for (const std::string& file : editor.modelFiles)
    {
        ImGui::Selectable(file.c_str());

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(kModelPayload, file.data(), file.size());
            ImGui::Text("%s", file.c_str());
            ImGui::EndDragDropSource();
        }
    }

    if (editor.modelFiles.empty()) ImGui::TextDisabled("No models yet: put .glb files in assets/models.");
}

void DrawLevelList(Editor& editor)
{
    if (ImGui::Button("+ New level")) editor.askNewLevel = true;
    ImGui::SameLine();
    if (ImGui::Button("Show folder")) OpenWithSystem(PathsOf(editor.project).levels);

    ImGui::SameLine();
    HelpMark("The levels in scene order: ChangeScene(index) in a script goes to that scene, the game starts on 0. "
             "The arrows move one, a new level goes at the end.");

    for (size_t i = 0; i < editor.levels.size(); i++)
    {
        const std::string level = editor.levels[i];
        bool current = level == editor.doc.path;
        std::string compiled = CompiledLevel(editor.project, level);
        bool stale = !FileExists(compiled) || IsNewer(level, compiled);
        int move = 0;

        ImGui::PushID((int)i);
        ImGui::TextDisabled("%2d", (int)i);
        ImGui::SameLine();
        if (ImGui::ArrowButton("##up", ImGuiDir_Up) && i > 0) move = -1;
        ImGui::SameLine();
        if (ImGui::ArrowButton("##down", ImGuiDir_Down) && i + 1 < editor.levels.size()) move = 1;
        ImGui::SameLine();

        if (ImGui::Selectable(FileName(level).c_str(), current, ImGuiSelectableFlags_AllowDoubleClick) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !current)
        {
            Defer(editor, [&editor, level]() { WhenSaved(editor, [&editor, level]() { OpenLevelFile(editor, level); }); });
        }

        ImGui::SameLine(300);
        ImGui::TextDisabled("%s", stale ? "not compiled since it changed" : "compiled");
        ImGui::PopID();

        if (move)
        {
            std::swap(editor.levels[i], editor.levels[i + move]);
            SaveSceneOrder(editor.project, editor.levels);
            editor.status = "Scene order saved";
            break;
        }
    }
}

void DrawAssets(Editor& editor)
{
    if (!ImGui::Begin("Assets"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::SmallButton("Refresh")) RefreshLists(editor);
    ImGui::SameLine();
    if (ImGui::SmallButton("Open assets folder")) OpenWithSystem(PathsOf(editor.project).assets);

    if (ImGui::BeginTabBar("##assets"))
    {
        if (ImGui::BeginTabItem("Textures")) { DrawTextureGrid(editor); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Models")) { DrawModelList(editor); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Scenes")) { DrawLevelList(editor); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Scripts")) { DrawClassList(editor); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::End();
}
