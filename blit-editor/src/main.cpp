// blit-editor: the Blit engine's level editor. SDL2 window, OpenGL 2.1 context, Dear ImGui with docking

#include <SDL.h>
#include <SDL_image.h>

#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <ImGuizmo.h>

#include "core/Files.h"
#include "ui/Editor.h"

void ApplyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 5);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.16f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.32f, 0.45f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.40f, 0.56f, 1.0f);
}

int main(int, char**)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blit Level Editor", SDL_GetError(), nullptr);
        return 1;
    }

    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow("Blit Level Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext context = window ? SDL_GL_CreateContext(window) : nullptr;

    if (!context || !gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) || !GLAD_GL_ARB_framebuffer_object)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Blit Level Editor",
                                 "OpenGL 2.1 with framebuffer objects is needed, and this computer's driver doesn't offer it.", window);
        return 1;
    }

    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

    // the window layout is remembered next to the settings, not in whatever folder the editor was started from
    std::string layout = JoinPath(SettingsFolder(), "layout.ini");
    io.IniFilename = layout.c_str();

    ApplyStyle();
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 120");

    Editor* editor = new Editor();   // big, and it outlives the loop's frames
    InitEditor(*editor);

    std::string title;

    while (!editor->quit)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                 event.window.windowID == SDL_GetWindowID(window)))
            {
                if (editor->screen == Screen::Editing) WhenSaved(*editor, [editor]() { editor->quit = true; });
                else editor->quit = true;
            }
        }

        // nothing to draw while minimised
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(50);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        PollRunner(*editor);

        switch (editor->screen)
        {
        case Screen::Setup: DrawSetup(*editor); break;
        case Screen::Hub: DrawHub(*editor); break;
        case Screen::Editing: DrawEditor(*editor); break;
        }

        editor->doc.EndFrame(ImGui::IsAnyItemActive() || ImGuizmo::IsUsing());

        if (WindowTitle(*editor) != title)
        {
            title = WindowTitle(*editor);
            SDL_SetWindowTitle(window, title.c_str());
        }

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ShutdownEditor(*editor);
    delete editor;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
