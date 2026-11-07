#pragma once

// making a new project from a template, like the VS Code extension's wizard: the same files, so either tool opens it

#include <string>
#include <vector>

#include "level/Level.h"
#include "project/Project.h"

struct ProjectTemplate
{
    const char* id;
    const char* title;
    const char* description;
    bool is2D;
};

const std::vector<ProjectTemplate>& ProjectTemplates();

struct NewProject
{
    std::string name;
    std::string location;       // the folder the project's folder goes in
    std::string templateId = "room";
    std::string engine;
    std::string tools;
};

// what's wrong with the values, "" when it can be made
std::string CheckNewProject(const NewProject& values);
bool CreateProject(const NewProject& values, Project& project, std::string& error);

// the levels the empty templates start with, also what File > New Level makes
Level StarterLevel3D();
Level StarterLevel2D();
// their pieces: a node with one box brush (corners in level space), a node with one element
Node BrushNode(const std::string& name, Vec3 lo, Vec3 hi, const std::string& texture, BrushRole role);
Node PointNode(const std::string& name, Vec3 at, ElementType type);
// ids 1, 2, 3... in tree order
Level Numbered(Level level);

// the template's main.bs: one of the engine's examples, pointed at levels/main.blvl
bool TemplateScript(const NewProject& values, bool is2D, std::string& script, std::string& error);
std::string ReplaceAll(std::string text, const std::string& from, const std::string& to);
// [from in the engine, to in the project] pairs
bool CopyAll(const std::string& engine, const std::string& root, const std::vector<std::pair<std::string, std::string>>& files,
             std::string& error);
