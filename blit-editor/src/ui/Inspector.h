#pragma once

// the inspector's pieces: Inspector.cpp draws the node and the level, ElementEditors.cpp each element,
// FaceEditor.cpp the faces of face mode

#include "ui/Editor.h"

bool DrawLevelSettings(Editor& editor);
bool DrawTransform(Editor& editor, Node& node);
bool DrawElements(Editor& editor, Node& node);
bool DrawNode(Editor& editor, Node& node);

// one element's fields, true when something changed
bool DrawElement(Editor& editor, Node& node, Element& element);
bool DrawBrushElement(Editor& editor, Node& node, Element& element);
// a new element of this type for the inspector's Add element, with what's sensible for this level
Element NewElementFor(Editor& editor, ElementType type);

// the selected faces' texture settings
bool DrawFaces(Editor& editor);
// the faces of FaceRefs that still exist
std::vector<Face*> SelectedFaces(Editor& editor);
