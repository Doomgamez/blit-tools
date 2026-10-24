"""Python port of blitfs.h: read and write BlitEngine BLITFS archives.

On disk an archive is just records back to back, no header or index:

    "BFS1"                 magic
    u32 LE  name_size
    u32 LE  size           uncompressed size
    u32 LE  compressed_size
    name    name_size bytes
    data    compressed_size bytes, zlib (what compress()/uncompress() use)

Unlike BlitFS_Open, a truncated or corrupt archive raises BlitFSError instead of
silently returning whatever was read before the damage.
"""
import os
import struct
import zlib
from dataclasses import dataclass

MAGIC = b"BFS1"
_RECORD_HEADER = struct.Struct("<4sIII")  # magic, name_size, size, compressed_size


class BlitFSError(Exception):
    pass


@dataclass
class FSInput:
    name: str
    bytelist: bytes

    @property
    def size(self):
        return len(self.bytelist)


class FS:
    def __init__(self, files=None):
        self.Filesystem: list[FSInput] = list(files or [])

    @property
    def size(self):
        return len(self.Filesystem)

    def Get(self, name):
        """First file with this name, like BlitFS_Get, or None."""
        for f in self.Filesystem:
            if f.name == name:
                return f

        return None

    def __iter__(self):
        return iter(self.Filesystem)


def _read_exact(file, count, what):
    data = file.read(count)

    if len(data) != count:
        raise BlitFSError(f"truncated archive while reading {what} (wanted {count} bytes, got {len(data)})")

    return data


def Open(path) -> FS:
    fs = FS()

    with open(path, "rb") as file:
        while True:
            offset = file.tell()
            header = file.read(_RECORD_HEADER.size)

            if not header:
                break                                   # clean end of archive

            if len(header) < _RECORD_HEADER.size:
                raise BlitFSError(f"truncated record header at offset {offset}")

            magic, name_size, size, compressed_size = _RECORD_HEADER.unpack(header)

            if magic != MAGIC:
                raise BlitFSError(f"bad magic {magic!r} at offset {offset} (expected {MAGIC!r})")

            name = _read_exact(file, name_size, "name").decode("utf-8", errors="replace")
            compressed = _read_exact(file, compressed_size, f"data of {name!r}")

            try:
                data = zlib.decompress(compressed)
            except zlib.error as e:
                raise BlitFSError(f"{name!r} at offset {offset}: zlib error: {e}") from None

            if len(data) != size:
                raise BlitFSError(f"{name!r}: header says {size} bytes, decompressed to {len(data)}")

            fs.Filesystem.append(FSInput(name, data))

    return fs


def ExtractFile(f: FSInput, out_dir):
    """Write one file under out_dir, keeping its folders. Returns the path, or None when the
    name would escape out_dir (e.g. '../../x'), since names come from untrusted archives."""
    root = os.path.abspath(out_dir)
    dest = os.path.abspath(os.path.join(root, f.name))

    if os.path.commonpath([root, dest]) != root or dest == root:
        return None

    os.makedirs(os.path.dirname(dest), exist_ok=True)

    with open(dest, "wb") as out:
        out.write(f.bytelist)

    return dest


def Extract(fs: FS, out_dir):
    """Extract every file; returns (written paths, skipped unsafe names)."""
    written = []
    skipped = []

    for f in fs:
        dest = ExtractFile(f, out_dir)

        if dest:
            written.append(dest)
        else:
            skipped.append(f.name)

    return written, skipped


def Save(path, fs: FS, level=9):
    """Write fs in the format BlitFS_Open reads."""
    with open(path, "wb") as file:
        for f in fs:
            name = f.name.encode("utf-8")
            compressed = zlib.compress(f.bytelist, level)

            file.write(_RECORD_HEADER.pack(MAGIC, len(name), len(f.bytelist), len(compressed)))
            file.write(name)
            file.write(compressed)
