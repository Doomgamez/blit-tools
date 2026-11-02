"""Compares a glTF document with what the engine supports.

The rules follow the comment at the top of src/Renderer/Model.h in the engine,
keep them in sync with it. Every check returns a list of (level, text) pairs.
"""
import os
import urllib.parse

from .Loader import Item, Items
from .Tree import SceneRoots

#ModelNode.name is char[64], one byte goes to the terminator
NODE_NAME_MAX = 63

SUPPORTED_ATTRIBUTES = ("POSITION", "NORMAL", "TEXCOORD_0", "COLOR_0")
COMPRESSION_EXTENSIONS = ("KHR_draco_mesh_compression", "EXT_meshopt_compression", "KHR_meshopt_compression")


def CheckDocument(doc):
    out = []

    if doc.get("asset", {}).get("version", "").split(".")[0] != "2":
        out.append(("error", "only glTF 2.0 is supported"))

    if not SceneRoots(doc):
        out.append(("error", "the default scene has no nodes, nothing will be drawn"))

    if Items(doc, "animations"):
        out.append(("warning", f"{len(doc['animations'])} animation(s) are ignored"))

    if Items(doc, "skins"):
        out.append(("warning", f"{len(doc['skins'])} skin(s) are ignored"))

    for name in doc.get("extensionsRequired", []):
        if name not in COMPRESSION_EXTENSIONS:
            out.append(("warning", f"required extension {name} is not implemented by the engine"))

    return out


def CheckNodes(doc, reached):
    nodes = Items(doc, "nodes")
    out = []
    names = {}

    for i, node in enumerate(nodes):
        name = node.get("name", "")

        if not name:
            if i in reached:
                out.append(("note", f"node {i} has no name, FindModelNode can not reach it"))
            continue

        names.setdefault(name, []).append(i)

        if len(name.encode("utf-8")) > NODE_NAME_MAX:
            out.append(("warning", f"node {name!r} is longer than {NODE_NAME_MAX} bytes and will be cut off"))

        if i in reached and "skin" in node:
            out.append(("warning", f"node {name!r} uses a skin, it will not deform"))

    for name, indices in names.items():
        if len(indices) > 1:
            out.append(("warning", f"node name {name!r} is used by {len(indices)} nodes: {indices}"))

    for i in range(len(nodes)):
        if i not in reached:
            label = nodes[i].get("name") or i
            out.append(("note", f"node {label!r} is not in the default scene and will not be drawn"))

    return out


def CheckPrimitive(doc, label, prim):
    out = []
    attributes = prim.get("attributes", {})
    accessors = Items(doc, "accessors")

    if "POSITION" not in attributes:
        out.append(("error", f"mesh {label!r} has a primitive without POSITION"))

    for name, index in attributes.items():
        if name not in SUPPORTED_ATTRIBUTES:
            out.append(("note", f"mesh {label!r} attribute {name} is ignored"))

        if Item(accessors, index) is None:
            out.append(("error", f"mesh {label!r} attribute {name} uses accessor {index}, which does not exist"))

    if "indices" in prim and Item(accessors, prim["indices"]) is None:
        out.append(("error", f"mesh {label!r} uses index accessor {prim['indices']}, which does not exist"))

    if "material" in prim and Item(Items(doc, "materials"), prim["material"]) is None:
        out.append(("error", f"mesh {label!r} uses material {prim['material']}, which does not exist"))

    if prim.get("targets"):
        out.append(("warning", f"mesh {label!r} has morph targets, they are ignored"))

    for name in COMPRESSION_EXTENSIONS:
        if name in prim.get("extensions", {}):
            out.append(("error", f"mesh {label!r} is compressed with {name}"))

    return out


def CheckMeshes(doc):
    out = []

    for i, mesh in enumerate(Items(doc, "meshes")):
        label = mesh.get("name") or i

        for prim in mesh.get("primitives", []):
            out += CheckPrimitive(doc, label, prim)

    return out


def CheckMaterials(doc):
    out = []

    for i, material in enumerate(Items(doc, "materials")):
        label = material.get("name") or i
        pbr = material.get("pbrMetallicRoughness", {})
        ignored = []

        if "metallicRoughnessTexture" in pbr or "metallicFactor" in pbr or "roughnessFactor" in pbr:
            ignored.append("metallic/roughness")

        for key, name in (("normalTexture", "normal map"), ("occlusionTexture", "occlusion"),
                          ("emissiveTexture", "emissive"), ("emissiveFactor", "emissive")):
            if key in material and name not in ignored:
                ignored.append(name)

        if ignored:
            out.append(("note", f"material {label!r}: {', '.join(ignored)} ignored"))

    return out


def CheckFiles(doc, path):
    out = []
    folder = os.path.dirname(os.path.abspath(path))

    for key in ("buffers", "images"):
        for item in Items(doc, key):
            uri = item.get("uri")

            if not uri or uri.startswith("data:"):
                continue

            name = urllib.parse.unquote(uri)

            if not os.path.isfile(os.path.join(folder, name)):
                out.append(("error", f"{key[:-1]} file {name!r} is missing, pack it into the archive with the model"))

    return out


def RunChecks(doc, path, tree_problems, reached):
    return ([("error", p) for p in tree_problems] + CheckDocument(doc) + CheckNodes(doc, reached)
            + CheckMeshes(doc) + CheckMaterials(doc) + CheckFiles(doc, path))
