#pragma once

// small ImGui pieces the inspector uses. each returns true when the value changed

#include <string>
#include <vector>

#include "level/Level.h"

// a 0-255 colour as a colour picker
bool Color255(const char* label, Vec3& color);
// a text field with a list of suggestions next to it (class names, files)
bool TextWithChoices(const char* label, std::string& value, const std::vector<std::string>& choices, const char* hint);
// key / value rows with add and remove
bool PairsTable(const char* id, Pairs& pairs);
// a texture name with a thumbnail, clicking it picks from the project's textures
struct Editor;
bool TextureField(Editor& editor, const char* label, std::string& texture);
// a little "?" with a tooltip
void HelpMark(const char* text);
