from dataclasses import dataclass

import BLITFS


@dataclass
class ViewerState:
    path: str | None = None             # archive currently open (None = new, never saved)
    fs: BLITFS.FS | None = None
    dirty: bool = False                 # in-memory archive differs from the file on disk

    def selected(self, index):
        """File for a 0-based list index, or None."""
        if self.fs is None or not 0 <= index < self.fs.size:
            return None

        return self.fs.Filesystem[index]

    def name_taken(self, name, except_index=-1):
        return any(f.name == name for i, f in enumerate(self.fs) if i != except_index)
