// the screens before a project is open: first-run setup (where homeinvasionsdl and blit-tools are), then choosing
// or creating a project

#include "ui/Editor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include "core/Dialogs.h"
#include "core/Files.h"
#include "project/Build.h"

const ImVec4 kProblemColor = ImVec4(1.0f, 0.55f, 0.45f, 1.0f);
const ImVec4 kGoodColor = ImVec4(0.5f, 0.9f, 0.55f, 1.0f);

// a full-window page with a title
bool BeginPage(const char* id, const char* title)
{
    ImGuiViewport* main = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(main->WorkPos);
    ImGui::SetNextWindowSize(main->WorkSize);
    bool open = ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextUnformatted(title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();
    return open;
}

// a text field with a Browse button that opens the folder picker. true when the folder changed
bool FolderField(const char* label, std::string& folder, const char* pickerTitle)
{
    bool changed = false;

    ImGui::PushID(label);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
    changed |= ImGui::InputText(label, &folder);
    ImGui::SameLine();

    if (ImGui::Button("Browse..."))
    {
        std::string picked = PickFolder(pickerTitle, folder);
        if (!picked.empty()) { folder = picked; changed = true; }
    }

    ImGui::PopID();
    return changed;
}

void Verdict(const std::string& problem, const char* good)
{
    if (problem.empty()) ImGui::TextColored(kGoodColor, "%s", good);
    else ImGui::TextColored(kProblemColor, "%s", problem.c_str());
}

void DrawSetup(Editor& editor)
{
    HubState& hub = editor.hub;

    if (BeginPage("##setup", "Blit Level Editor setup"))
    {
        ImGui::TextWrapped("The editor needs two folders on this computer. homeinvasionsdl is the engine: projects build "
                           "it, and its blitlevel compiles levels. blit-tools is where this editor lives.");
        ImGui::Spacing();

        ImGui::Text("homeinvasionsdl");
        FolderField("##engine", hub.setupEngine, "Choose the homeinvasionsdl folder");
        std::string engineProblem = CheckEngineFolder(hub.setupEngine);
        Verdict(engineProblem, "Found the engine");
        ImGui::Spacing();

        ImGui::Text("blit-tools");
        FolderField("##tools", hub.setupTools, "Choose the blit-tools folder");
        std::string toolsProblem = CheckToolsFolder(hub.setupTools);
        Verdict(toolsProblem, "Found blit-tools");
        ImGui::Spacing();

        ImGui::Text("MinGW toolchain (bin folder with gcc, empty for C:/msys64/mingw64/bin)");
        FolderField("##toolchain", hub.setupToolchain, "Choose MinGW's bin folder");
        Settings probe = editor.settings;
        probe.toolchain = hub.setupToolchain;
        std::string toolchain = FindToolchain(probe);
        Verdict(!toolchain.empty() && FileExists(JoinPath(toolchain, Executable("gcc"))) ? "" :
                "No gcc found there: building the engine won't work until it is", "Found gcc");
        ImGui::Spacing();
        ImGui::Spacing();

        bool ok = engineProblem.empty() && toolsProblem.empty();
        if (!ok) ImGui::BeginDisabled();

        if (ImGui::Button("Continue", ImVec2(160, 0)))
        {
            editor.settings.engine = CleanPath(hub.setupEngine);
            editor.settings.tools = CleanPath(hub.setupTools);
            editor.settings.toolchain = CleanPath(hub.setupToolchain);
            SaveSettings(editor.settings);
            editor.screen = Screen::Hub;
        }

        if (!ok) ImGui::EndDisabled();
    }

    ImGui::End();
}

void DrawRecent(Editor& editor)
{
    ImGui::Text("Recent projects");
    ImGui::BeginChild("##recent", ImVec2(0, 260), ImGuiChildFlags_Borders);
    std::string open;

    for (const std::string& root : editor.settings.recent)
    {
        bool there = IsProjectFolder(root);
        ImGui::PushID(root.c_str());

        if (!there) ImGui::BeginDisabled();
        if (ImGui::Selectable(FileName(root).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) open = root;
        if (!there) ImGui::EndDisabled();

        ImGui::SameLine(220);
        ImGui::TextDisabled("%s%s", root.c_str(), there ? "" : "  (gone)");
        ImGui::PopID();
    }

    if (editor.settings.recent.empty()) ImGui::TextDisabled("Nothing yet: open a project folder or make a new one.");
    ImGui::EndChild();

    if (ImGui::Button("Open a project folder..."))
    {
        open = PickFolder("Open a Blit project", editor.settings.recent.empty() ? "" : ParentFolder(editor.settings.recent.front()));
    }

    if (!open.empty())
    {
        editor.hub.error.clear();
        OpenProjectFolder(editor, open);
    }
}

void DrawNewProject(Editor& editor)
{
    NewProject& form = editor.hub.form;

    if (form.engine.empty()) form.engine = editor.settings.engine;
    if (form.tools.empty()) form.tools = editor.settings.tools;
    if (form.location.empty() && !editor.settings.recent.empty()) form.location = ParentFolder(editor.settings.recent.front());

    ImGui::Text("New project");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##name", "Name", &form.name);
    FolderField("##location", form.location, "Where to put the project");

    for (const ProjectTemplate& t : ProjectTemplates())
    {
        if (ImGui::RadioButton(t.title, form.templateId == t.id)) form.templateId = t.id;
        ImGui::Indent();
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", t.description);
        ImGui::PopTextWrapPos();
        ImGui::Unindent();
    }

    std::string problem = CheckNewProject(form);
    if (!problem.empty() && (!form.name.empty() || !form.location.empty())) ImGui::TextColored(kProblemColor, "%s", problem.c_str());
    else if (problem.empty()) ImGui::TextDisabled("%s", JoinPath(form.location, form.name).c_str());

    if (!problem.empty()) ImGui::BeginDisabled();

    if (ImGui::Button("Create", ImVec2(160, 0)))
    {
        Project project;
        std::string error;

        if (CreateProject(form, project, error)) OpenProjectFolder(editor, project.root);
        else editor.hub.error = error;
    }

    if (!problem.empty()) ImGui::EndDisabled();
}

void DrawHub(Editor& editor)
{
    if (BeginPage("##hub", "Blit Level Editor"))
    {
        ImGui::TextDisabled("homeinvasionsdl: %s    blit-tools: %s", editor.settings.engine.c_str(), editor.settings.tools.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Change")) editor.screen = Screen::Setup;
        ImGui::Spacing();

        if (!editor.hub.error.empty()) ImGui::TextColored(kProblemColor, "%s", editor.hub.error.c_str());

        if (ImGui::BeginTable("##hub", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            DrawRecent(editor);
            ImGui::TableNextColumn();
            DrawNewProject(editor);
            ImGui::EndTable();
        }
    }

    ImGui::End();
}
