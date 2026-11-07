// the build log: what cmake, blitscriptc, blitlevel, blitfs_pack and the game print

#include <imgui.h>

#include "ui/Editor.h"

ImVec4 LogColor(LogKind kind)
{
    switch (kind)
    {
    case LogKind::Step: return ImVec4(0.55f, 0.75f, 1.0f, 1.0f);
    case LogKind::Error: return ImVec4(1.0f, 0.5f, 0.45f, 1.0f);
    case LogKind::Success: return ImVec4(0.5f, 0.9f, 0.55f, 1.0f);
    case LogKind::Output: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    default: return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    }
}

void DrawConsole(Editor& editor)
{
    if (!ImGui::Begin("Console"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::SmallButton("Clear")) LogClear(editor.runner.log);
    ImGui::SameLine();

    if (editor.runner.busy)
    {
        if (ImGui::SmallButton("Stop")) StopSteps(editor.runner);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", RunnerStatus(editor.runner).c_str());
    }
    else
    {
        ImGui::TextDisabled("F5 plays the level, F7 compiles it");
    }

    ImGui::Separator();
    ImGui::BeginChild("##log", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    int version = 0;
    std::vector<LogLine> lines = LogCopy(editor.runner.log, version);
    bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f;
    ImGuiListClipper clipper;

    clipper.Begin((int)lines.size());

    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, LogColor(lines[(size_t)i].kind));
            ImGui::TextUnformatted(lines[(size_t)i].text.c_str());
            ImGui::PopStyleColor();
        }
    }

    // follows new lines unless the user scrolled up to read
    if (atBottom) ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}
