#include "editor/Document.h"

#include <algorithm>

#include "core/Files.h"
#include "level/LevelFile.h"

constexpr size_t kHistoryLimit = 200;

void Document::New(bool is2D)
{
    level = Level();
    level.is2D = is2D;
    level.texelsPerMetre = is2D ? 1.0f : 32.0f;
    path.clear();
    dirty = false;
    selection.clear();
    faces.clear();
    version++;
    ResetHistory();
}

bool Document::Load(const std::string& file, std::string& error)
{
    Level loaded;

    if (!LoadLevel(file, loaded, error))
    {
        return false;
    }

    level = std::move(loaded);
    path = file;
    dirty = false;
    selection.clear();
    faces.clear();
    version++;
    ResetHistory();
    return true;
}

bool Document::Save()
{
    if (path.empty() || !SaveLevel(path, level))
    {
        return false;
    }

    dirty = false;
    return true;
}

bool Document::SaveAs(const std::string& file)
{
    path = file;
    return Save();
}

std::string Document::Title() const
{
    return (path.empty() ? std::string("untitled") : FileName(path)) + (dirty ? " *" : "");
}

std::string Document::Snapshot() const
{
    return JsonWrite(LevelToJson(level));
}

void Document::Restore(const std::string& snapshot)
{
    Json json;
    std::string error;

    if (JsonParse(snapshot, json, error))
    {
        LevelFromJson(json, level, error);
    }

    dirty = true;
    version++;
    PruneSelection();
}

void Document::ResetHistory()
{
    history.assign(1, Snapshot());
    historyAt = 0;
    pending = false;
}

void Document::Changed()
{
    dirty = true;
    pending = true;
    version++;
}

void Document::EndFrame(bool editing)
{
    if (!pending || editing)
    {
        return;
    }

    history.resize((size_t)historyAt + 1);
    history.push_back(Snapshot());

    if (history.size() > kHistoryLimit)
    {
        history.erase(history.begin());
    }

    historyAt = (int)history.size() - 1;
    pending = false;
}

bool Document::CanUndo() const { return historyAt > 0; }
bool Document::CanRedo() const { return historyAt + 1 < (int)history.size(); }

void Document::Undo()
{
    if (CanUndo())
    {
        Restore(history[(size_t)--historyAt]);
    }
}

void Document::Redo()
{
    if (CanRedo())
    {
        Restore(history[(size_t)++historyAt]);
    }
}

Node* Document::Primary()
{
    return selection.empty() ? nullptr : FindNode(level.nodes, selection.back());
}

bool Document::IsSelected(int id) const
{
    return std::find(selection.begin(), selection.end(), id) != selection.end();
}

void Document::Select(int id, bool additive)
{
    if (!additive)
    {
        selection.clear();
    }

    auto found = std::find(selection.begin(), selection.end(), id);

    if (found != selection.end())
    {
        if (additive)
        {
            selection.erase(found); // ctrl+click on a selected node takes it out
            return;
        }
    }

    if (id)
    {
        selection.push_back(id);
    }
}

void Document::ClearSelection()
{
    selection.clear();
    faces.clear();
}

void Document::PruneSelection()
{
    selection.erase(std::remove_if(selection.begin(), selection.end(),
                                   [&](int id) { return !FindNode(level.nodes, id); }), selection.end());
    faces.erase(std::remove_if(faces.begin(), faces.end(),
                               [&](const FaceRef& f) { return !FindNode(level.nodes, f.node); }), faces.end());
}

std::vector<int> Document::TopSelection() const
{
    std::vector<int> out;

    for (int id : selection)
    {
        bool underSelected = false;

        for (int other : selection)
        {
            underSelected |= other != id && IsDescendant(level.nodes, other, id);
        }

        if (!underSelected && FindNode(level.nodes, id))
        {
            out.push_back(id);
        }
    }

    return out;
}

int Document::AddNode(const Node& node, int parent, bool select)
{
    Node copy = node;
    Node* under = parent ? FindNode(level.nodes, parent) : nullptr;

    Reassign(copy, level);
    copy.name = UniqueName(level, copy.name);
    (under ? under->children : level.nodes).push_back(copy);

    if (select)
    {
        Select(copy.id, false);
    }

    Changed();
    return copy.id;
}

void Document::DeleteSelected()
{
    for (int id : TopSelection())
    {
        size_t index = 0;
        std::vector<Node>* siblings = FindSiblings(level.nodes, id, index);

        if (siblings)
        {
            siblings->erase(siblings->begin() + (long)index);
        }
    }

    ClearSelection();
    Changed();
}

void Document::DuplicateSelected()
{
    std::vector<int> made;

    for (int id : TopSelection())
    {
        const Node* node = FindNode(level.nodes, id);
        int parent = FindParentId(level.nodes, id);

        if (node)
        {
            made.push_back(AddNode(*node, parent > 0 ? parent : 0, false));
        }
    }

    selection = made;
    Changed();
}

bool Document::Reparent(int id, int parent, int index)
{
    if (id == parent || (parent && IsDescendant(level.nodes, id, parent)) || !FindNode(level.nodes, id))
    {
        return false; // a node can't go under itself
    }

    Mat4 world = NodeWorld(level.nodes, id);
    size_t at = 0;
    std::vector<Node>* siblings = FindSiblings(level.nodes, id, at);
    Node moved = (*siblings)[at];

    siblings->erase(siblings->begin() + (long)at);

    // keeps its place in the level: its new local matrix is the old world one seen from the new parent
    Mat4 parentWorld = parent ? NodeWorld(level.nodes, parent) : Mat4();
    Mat4Decompose(Mat4Inverse(parentWorld) * world, moved.position, moved.rotation, moved.scale);

    Node* under = parent ? FindNode(level.nodes, parent) : nullptr;
    std::vector<Node>& list = under ? under->children : level.nodes;
    size_t place = index < 0 || index > (int)list.size() ? list.size() : (size_t)index;

    list.insert(list.begin() + (long)place, moved);
    Changed();
    return true;
}

void Document::GroupSelected()
{
    std::vector<int> top = TopSelection();
    Node group;
    group.name = "Group";

    if (top.empty())
    {
        return;
    }

    int parent = std::max(FindParentId(level.nodes, top.front()), 0);
    int id = AddNode(group, parent, false);

    for (int child : top)
    {
        Reparent(child, id, -1);
    }

    Select(id, false);
}

std::string Document::CopySelection() const
{
    Json json = JsonObject();
    Json& nodes = json["blitnodes"] = JsonArray();

    for (int id : TopSelection())
    {
        nodes.items.push_back(NodeToJson(*FindNode(level.nodes, id)));
    }

    return JsonWrite(json);
}

bool Document::Paste(const std::string& text)
{
    Json json;
    std::string error;
    const Json* nodes = nullptr;

    if (!JsonParse(text, json, error) || !(nodes = json.Find("blitnodes")) || !nodes->IsArray())
    {
        return false;
    }

    Node* primary = Primary();
    int parent = primary ? std::max(FindParentId(level.nodes, primary->id), 0) : 0;
    std::vector<int> made;

    for (const Json& item : nodes->items)
    {
        Level scratch;
        made.push_back(AddNode(NodeFromJson(item, scratch), parent, false));
    }

    selection = made;
    return !made.empty();
}
