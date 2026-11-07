#pragma once

// the level being edited: its file, selection and undo history. every change goes through Changed(), the history
// keeps a snapshot once the user lets go of what they're editing (a drag, a text field), so one drag is one undo

#include <string>
#include <vector>

#include "level/Level.h"

// a face of a brush, for face mode (texturing)
struct FaceRef
{
    int node = 0;
    int element = -1;
    int brush = -1;
    int face = -1;

    bool operator==(const FaceRef& o) const
    {
        return node == o.node && element == o.element && brush == o.brush && face == o.face;
    }
};

class Document
{
public:
    Level level;
    std::string path;           // empty for a level that hasn't been saved
    bool dirty = false;
    int version = 0;            // goes up with every change, what the viewport rebuilds its meshes on

    std::vector<int> selection; // node ids, the last one is the primary
    std::vector<FaceRef> faces; // selected faces in face mode

    void New(bool is2D);
    bool Load(const std::string& file, std::string& error);
    bool Save();
    bool SaveAs(const std::string& file);
    std::string Title() const;

    // after changing level: marks it unsaved, and a snapshot is taken at EndFrame
    void Changed();
    // editing: the user still holds a widget or a handle, the snapshot waits
    void EndFrame(bool editing);
    bool CanUndo() const;
    bool CanRedo() const;
    void Undo();
    void Redo();

    Node* Primary();
    bool IsSelected(int id) const;
    void Select(int id, bool additive);
    void ClearSelection();
    // drops ids of nodes that are gone (after an undo)
    void PruneSelection();

    // a copy of node added under parent (0: the top), with fresh ids. the new node's id
    int AddNode(const Node& node, int parent, bool select = true);
    void DeleteSelected();
    void DuplicateSelected();
    // moves the node under another (0: the top) at index (-1: last), keeping where it is in the level
    bool Reparent(int id, int parent, int index);
    // a new node holding the selection
    void GroupSelected();

    // the selection as JSON text for the clipboard, and nodes from it
    std::string CopySelection() const;
    bool Paste(const std::string& text);

    std::vector<std::string> history;
    int historyAt = -1;
    bool pending = false;

    void ResetHistory();
    void Restore(const std::string& snapshot);
    std::string Snapshot() const;
    // the selected nodes without any whose ancestor is selected too
    std::vector<int> TopSelection() const;
};
