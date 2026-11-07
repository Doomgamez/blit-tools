#include "ui/Editor.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include "core/Dialogs.h"
#include "core/Files.h"
#include "project/Build.h"

void AddNodeOfKind(Editor& editor, NodeKind kind, int parent)
{
    Node node = MakeNodeOfKind(kind, editor.doc.level.is2D, GridStep(editor), editor.currentTexture);

    // at the middle of the view, on the grid. under a parent it starts at the parent's origin
    if (!parent)
    {
        float w = (float)editor.viewport.width, h = (float)editor.viewport.height;
        Vec3 at = PointOnGrid(editor.camera, w * 0.5f, h * 0.5f, w, h);
        node.position = editor.snap ? Snap(at, GridStep(editor)) : at;

        if (editor.doc.level.is2D) node.position.z = 0;
    }

    editor.doc.AddNode(node, parent);
    editor.faceMode = false;
}

void Defer(Editor& editor, std::function<void()> action)
{
    editor.deferred.push_back(std::move(action));
}

void RunDeferred(Editor& editor)
{
    std::vector<std::function<void()>> actions;
    actions.swap(editor.deferred);

    for (auto& action : actions) action();
}

void DrawAddMenu(Editor& editor, int parent)
{
    const char* section = "";
    bool is2D = editor.doc.level.is2D;

    for (const NodeKindInfo& info : NodeKinds())
    {
        if ((info.only2D && !is2D) || (info.only3D && is2D)) continue;

        if (std::string(section) != info.section)
        {
            if (section[0]) ImGui::Separator();
            section = info.section;
            ImGui::TextDisabled("%s", section);
        }

        if (ImGui::MenuItem(info.title))
        {
            NodeKind kind = info.kind;
            Defer(editor, [&editor, kind, parent]() { AddNodeOfKind(editor, kind, parent); });
        }
    }
}

void DrawMenuBar(Editor& editor)
{
    Document& doc = editor.doc;

    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New level...")) editor.askNewLevel = true;

        if (ImGui::BeginMenu("Open level", !editor.levels.empty()))
        {
            for (const std::string& level : editor.levels)
            {
                if (ImGui::MenuItem(FileName(level).c_str(), nullptr, level == doc.path))
                {
                    WhenSaved(editor, [&editor, level]() { OpenLevelFile(editor, level); });
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Save level", "Ctrl+S")) SaveCurrentLevel(editor);
        ImGui::Separator();
        if (ImGui::MenuItem("Open project folder")) OpenWithSystem(editor.project.root);
        if (ImGui::MenuItem("Close project")) WhenSaved(editor, [&editor]() { CloseProject(editor); });
        if (ImGui::MenuItem("Folders and toolchain...")) WhenSaved(editor, [&editor]() { CloseProject(editor); editor.screen = Screen::Setup; });
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) WhenSaved(editor, [&editor]() { editor.quit = true; });
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, doc.CanUndo())) doc.Undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, doc.CanRedo())) doc.Redo();
        ImGui::Separator();
        bool any = !doc.selection.empty();
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, any)) { ImGui::SetClipboardText(doc.CopySelection().c_str()); doc.DeleteSelected(); }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, any)) ImGui::SetClipboardText(doc.CopySelection().c_str());
        if (ImGui::MenuItem("Paste", "Ctrl+V")) { const char* text = ImGui::GetClipboardText(); if (text) doc.Paste(text); }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, any)) doc.DuplicateSelected();
        if (ImGui::MenuItem("Delete", "Del", false, any)) doc.DeleteSelected();
        if (ImGui::MenuItem("Group", "Ctrl+G", false, any)) doc.GroupSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Select none", "Esc")) doc.ClearSelection();
        if (ImGui::MenuItem("Face mode (texturing)", "G", editor.faceMode)) editor.faceMode = !editor.faceMode;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Add"))
    {
        DrawAddMenu(editor, 0);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        for (int k = 0; k < (int)ViewKind::Count; k++)
        {
            bool allowed = doc.level.is2D ? k == (int)ViewKind::TwoD : k != (int)ViewKind::TwoD;
            if (ImGui::MenuItem(ViewTitle((ViewKind)k), nullptr, editor.camera.kind == (ViewKind)k, allowed))
            {
                editor.camera.kind = (ViewKind)k;
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Reset camera")) ResetCamera(editor.camera, doc.level.is2D);
        ImGui::MenuItem("Preview lighting", nullptr, &editor.lighting, !doc.level.is2D);
        if (ImGui::MenuItem("Show volumes", nullptr, &editor.sceneOptions.showVolumes)) doc.version++;
        if (ImGui::MenuItem("Show icons", nullptr, &editor.sceneOptions.showIcons)) doc.version++;
        ImGui::MenuItem("ImGui demo", nullptr, &editor.showDemo);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Build"))
    {
        bool busy = editor.runner.busy;
        if (ImGui::MenuItem("Play this level", "F5", false, !busy)) PlayLevel(editor);
        if (ImGui::MenuItem("Play the game", "Shift+F5", false, !busy)) PlayGame(editor);
        if (ImGui::MenuItem("Compile this level", "F7", false, !busy)) CompileCurrentLevel(editor);
        if (ImGui::MenuItem("Build the project", "Ctrl+B", false, !busy)) BuildProject(editor, false);
        if (ImGui::MenuItem("Rebuild the engine", nullptr, false, !busy)) BuildProject(editor, true);
        if (ImGui::MenuItem("Stop", "Shift+Esc", false, busy)) StopSteps(editor.runner);
        ImGui::Separator();
        if (ImGui::MenuItem("Open the build folder")) OpenWithSystem(PathsOf(editor.project).output);
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void ToolButton(Editor& editor, const char* label, Tool tool, const char* tip)
{
    bool active = editor.tool == tool;

    if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(label)) editor.tool = tool;
    if (active) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    ImGui::SameLine();
}

void DrawToolbar(Editor& editor)
{
    ToolButton(editor, "Select", Tool::Select, "Click to select (Q)");
    ToolButton(editor, "Move", Tool::Move, "Move handles (W)");
    ToolButton(editor, "Rotate", Tool::Rotate, "Rotate handles (E)");
    ToolButton(editor, "Scale", Tool::Scale, "Scale the node (R)");
    ToolButton(editor, "Resize", Tool::Resize, "Resize brushes by their sides, textures stay put (T)");

    ImGui::TextDisabled("|"); ImGui::SameLine();
    ImGui::Checkbox("World", &editor.worldSpace); ImGui::SameLine();
    ImGui::Checkbox("Snap", &editor.snap); ImGui::SameLine();

    if (editor.doc.level.is2D)
    {
        ImGui::Checkbox("Smart", &editor.canvas.smart);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap to other nodes' sides and middles, and to guides");
        ImGui::SameLine();
    }

    ImGui::SetNextItemWidth(70);

    if (editor.doc.level.is2D)
    {
        ImGui::DragFloat("##grid", &editor.gridStep2D, 1.0f, 1.0f, 256.0f, "%.0f px");
    }
    else
    {
        const float steps[] = { 0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
        char current[32];
        snprintf(current, sizeof(current), "%g m", editor.gridStep);

        if (ImGui::BeginCombo("##grid", current))
        {
            for (float s : steps)
            {
                char label[32];
                snprintf(label, sizeof(label), "%g m", s);
                if (ImGui::Selectable(label, s == editor.gridStep)) editor.gridStep = s;
            }

            ImGui::EndCombo();
        }
    }

    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid size ([ and ] change it)");
    ImGui::SameLine();
    ImGui::Checkbox("Faces", &editor.faceMode);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Face mode: click faces to texture them (G)");
    ImGui::SameLine();
    ImGui::TextDisabled("|"); ImGui::SameLine();

    bool busy = editor.runner.busy;
    if (busy) ImGui::BeginDisabled();
    if (ImGui::Button("Play")) PlayLevel(editor);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Build and run the game on this level (F5)");
    ImGui::SameLine();
    if (ImGui::Button("Compile")) CompileCurrentLevel(editor);
    if (busy) ImGui::EndDisabled();

    if (busy)
    {
        ImGui::SameLine();
        if (ImGui::Button("Stop")) StopSteps(editor.runner);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", RunnerStatus(editor.runner).c_str());
    }
}

void DrawDialogs(Editor& editor)
{
    if (editor.askSave) { ImGui::OpenPopup("Unsaved changes"); editor.askSave = false; }
    if (editor.askNewLevel) { ImGui::OpenPopup("New level"); editor.askNewLevel = false; editor.newLevel2D = editor.doc.level.is2D; }

    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s has changes that aren't saved.", editor.doc.Title().c_str());

        if (ImGui::Button("Save"))
        {
            ImGui::CloseCurrentPopup();
            if (SaveCurrentLevel(editor) && editor.afterSave) editor.afterSave();
        }

        ImGui::SameLine();
        if (ImGui::Button("Don't save")) { ImGui::CloseCurrentPopup(); editor.doc.dirty = false; if (editor.afterSave) editor.afterSave(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("New level", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Name", &editor.newLevelName);
        ImGui::TextDisabled("levels/%s.blvl, LoadLevel \"%s.blvl\" loads it", editor.newLevelName.c_str(), editor.newLevelName.c_str());
        if (ImGui::RadioButton("3D", !editor.newLevel2D)) editor.newLevel2D = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("2D", editor.newLevel2D)) editor.newLevel2D = true;

        std::string problem = CheckProjectName(editor.newLevelName);
        if (!problem.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", problem.c_str());

        if (ImGui::Button("Create") && problem.empty())
        {
            ImGui::CloseCurrentPopup();
            std::string name = editor.newLevelName;
            bool is2D = editor.newLevel2D;
            WhenSaved(editor, [&editor, name, is2D]() { CreateLevel(editor, name, is2D); });
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void DrawEditor(Editor& editor)
{
    ImGuiViewport* main = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(main->WorkPos);
    ImGui::SetNextWindowSize(main->WorkSize);
    ImGui::SetNextWindowViewport(main->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##editor", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar();

    DrawMenuBar(editor);

    ImGuiID dock = ImGui::GetID("EditorDock");

    // the first time: hierarchy left, inspector right, viewport in the middle, assets and console below
    if (!ImGui::DockBuilderGetNode(dock))
    {
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, main->WorkSize);
        ImGuiID center = dock;
        ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
        ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);
        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Viewport", center);
        ImGui::DockBuilderDockWindow("Assets", bottom);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderFinish(dock);
    }

    ImGui::DockSpace(dock);
    ImGui::End();

    DrawHierarchy(editor);
    DrawViewport(editor);
    DrawInspector(editor);
    DrawAssets(editor);
    DrawConsole(editor);
    DrawDialogs(editor);
    HandleShortcuts(editor);
    RunDeferred(editor);

    if (editor.showDemo) ImGui::ShowDemoWindow(&editor.showDemo);
}
