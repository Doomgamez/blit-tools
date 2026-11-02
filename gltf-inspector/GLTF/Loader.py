"""Reads .gltf and .glb files into their JSON document."""
import json
import struct

GLB_MAGIC = 0x46546C67
GLB_CHUNK_JSON = 0x4E4F534A


class GltfError(Exception):
    pass


def ReadGlb(data):
    if len(data) < 12:
        raise GltfError("file is too small to be a .glb")

    magic, version, length = struct.unpack_from("<III", data, 0)

    if magic != GLB_MAGIC:
        raise GltfError("not a .glb, bad magic")

    if version != 2:
        raise GltfError(f"unsupported .glb version {version}, the engine reads version 2")

    if length > len(data):
        raise GltfError(f"truncated .glb, header says {length} bytes but the file has {len(data)}")

    if length < 20:
        raise GltfError("truncated .glb, no JSON chunk")

    size, kind = struct.unpack_from("<II", data, 12)

    if kind != GLB_CHUNK_JSON:
        raise GltfError("the first .glb chunk is not JSON")

    if 20 + size > length:
        raise GltfError("the JSON chunk runs past the end of the file")

    return data[20:20 + size]


def Load(path):
    with open(path, "rb") as fh:
        data = fh.read()

    is_glb = data[:4] == b"glTF" or path.lower().endswith(".glb")
    text = ReadGlb(data) if is_glb else data

    try:
        doc = json.loads(text)
    except ValueError as e:
        raise GltfError(f"bad JSON: {e}")

    if not isinstance(doc, dict):
        raise GltfError("the top level of a glTF file has to be an object")

    return doc


def Items(doc, key):
    value = doc.get(key)
    return value if isinstance(value, list) else []


def Item(items, index):
    return items[index] if isinstance(index, int) and 0 <= index < len(items) else None
