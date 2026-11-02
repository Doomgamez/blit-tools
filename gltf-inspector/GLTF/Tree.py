"""Mesh sizes and the node tree of the default scene."""
from .Loader import Item, Items


def PrimitiveVertices(doc, prim):
    index = prim.get("attributes", {}).get("POSITION")
    accessor = Item(Items(doc, "accessors"), index)
    return accessor.get("count", 0) if accessor else 0


def PrimitiveTriangles(doc, prim):
    accessor = Item(Items(doc, "accessors"), prim.get("indices"))
    count = accessor.get("count", 0) if accessor else PrimitiveVertices(doc, prim)
    mode = prim.get("mode", 4)

    if mode == 4:
        return count // 3

    if mode in (5, 6):
        return max(count - 2, 0)

    return 0


def MeshStats(doc, mesh):
    prims = mesh.get("primitives", []) if mesh else []

    return {
        "primitives": len(prims),
        "vertices": sum(PrimitiveVertices(doc, p) for p in prims),
        "triangles": sum(PrimitiveTriangles(doc, p) for p in prims),
    }


def SceneRoots(doc):
    scenes = Items(doc, "scenes")
    scene = Item(scenes, doc.get("scene", 0))
    return scene.get("nodes", []) if scene else []


def BuildTree(doc):
    """Returns the tree of the default scene, the indices it reaches and any problems."""
    nodes = Items(doc, "nodes")
    meshes = Items(doc, "meshes")
    seen = set()
    problems = []

    def Visit(index):
        node = Item(nodes, index)

        if node is None:
            problems.append(f"node index {index} does not exist")
            return None

        if index in seen:
            problems.append(f"node {index} is reached twice, the hierarchy is not a tree")
            return None

        seen.add(index)
        out = {"index": index, "name": node.get("name", "")}

        if "mesh" in node:
            mesh = Item(meshes, node["mesh"])
            out["mesh"] = mesh.get("name", f"mesh {node['mesh']}") if mesh else f"missing mesh {node['mesh']}"
            out.update(MeshStats(doc, mesh))

            if mesh is None:
                problems.append(f"node {index} uses mesh {node['mesh']}, which does not exist")

        children = (Visit(c) for c in node.get("children", []))
        out["children"] = [c for c in children if c]
        return out

    roots = (Visit(r) for r in SceneRoots(doc))
    return [r for r in roots if r], seen, problems
