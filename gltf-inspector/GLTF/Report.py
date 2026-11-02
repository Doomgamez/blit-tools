"""Builds the inspection report and prints it."""
from .Checks import RunChecks
from .Loader import GltfError, Items, Load
from .Tree import BuildTree, MeshStats


def BuildReport(path):
    doc = Load(path)
    tree, reached, problems = BuildTree(doc)
    meshes = [MeshStats(doc, m) for m in Items(doc, "meshes")]

    return {
        "file": path,
        "generator": doc.get("asset", {}).get("generator", ""),
        "counts": {
            "nodes": len(Items(doc, "nodes")),
            "meshes": len(meshes),
            "materials": len(Items(doc, "materials")),
            "textures": len(Items(doc, "textures")),
            "animations": len(Items(doc, "animations")),
            "skins": len(Items(doc, "skins")),
        },
        "triangles": sum(m["triangles"] for m in meshes),
        "tree": tree,
        "issues": [{"level": level, "text": text} for level, text in RunChecks(doc, path, problems, reached)],
    }


def Inspect(path):
    try:
        return BuildReport(path)
    except (AttributeError, TypeError, KeyError) as e:
        raise GltfError(f"malformed glTF: {e}")


def PrintNode(node, prefix, last):
    branch = "`-- " if last else "|-- "
    label = node["name"] or f"<unnamed {node['index']}>"

    if "mesh" in node:
        label += f"  [{node['mesh']}: {node['triangles']} tris, {node['primitives']} prim(s)]"

    print(prefix + branch + label)

    children = node["children"]

    for i, child in enumerate(children):
        PrintNode(child, prefix + ("    " if last else "|   "), i == len(children) - 1)


def PrintReport(report):
    print(report["file"])

    if report["generator"]:
        print(f"generator: {report['generator']}")

    print(", ".join(f"{count} {name}" for name, count in report["counts"].items()))
    print(f"{report['triangles']} triangles in the file's meshes")

    print("\nnodes:")

    for i, root in enumerate(report["tree"]):
        PrintNode(root, "", i == len(report["tree"]) - 1)

    if not report["tree"]:
        print("(none in the default scene)")

    print("\nissues:")

    for issue in report["issues"]:
        print(f"{issue['level']:>7}: {issue['text']}")

    if not report["issues"]:
        print("none")
