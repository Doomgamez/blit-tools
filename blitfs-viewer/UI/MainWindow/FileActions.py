"""Per-file actions: preview, open, extract, rename, delete, replace, and the right-click menu."""
import atexit
import os
import shutil
import tempfile

import BLITFS
from UI import Dialogs, Shell
from UI.Components.ContextMenu import DIVIDER
from UI.Events import UIEvent
from UI.MainWindow.Context import MainWindowContext


class FileActions:
    def __init__(self, ctx: MainWindowContext):
        self.ctx = ctx
        self._temp_dir = None

        bus = ctx.bus
        bus.on(UIEvent.FILE_SELECTED, self.preview)
        bus.on(UIEvent.FILE_ACTIVATED, self.open_default)
        bus.on(UIEvent.CONTEXT_MENU_REQUESTED, self.context_menu)
        bus.on(UIEvent.OPEN_FILE_REQUESTED, self.open_default)
        bus.on(UIEvent.EXTRACT_FILE_REQUESTED, self.extract)
        bus.on(UIEvent.RENAME_REQUESTED, self.rename)
        bus.on(UIEvent.DELETE_REQUESTED, self.delete)
        bus.on(UIEvent.REPLACE_REQUESTED, self.replace)

    def _file(self, index):
        return self.ctx.state.selected(index)

    def _changed(self, keep_index):
        self.ctx.state.dirty = True
        self.ctx.bus.emit(UIEvent.ARCHIVE_CHANGED, self.ctx.state, keep_index)

    # --- view -----------------------------------------------------------------

    def preview(self, index):
        f = self._file(index)

        if f is None:
            if self.ctx.state.fs is None:
                self.ctx.preview.clear()
            else:
                self.ctx.preview.clear("Select a file to preview it.")

            return

        how = self.ctx.preview.show_file(f)
        self.ctx.status.set(f"{f.name}  |  {f.size:,} bytes  |  {how}")

    def context_menu(self, index):
        f = self._file(index)
        has_file = f is not None
        has_archive = self.ctx.state.fs is not None

        def for_file(event):
            return lambda: self.ctx.bus.emit(event, index)

        def for_archive(event):
            return lambda: self.ctx.bus.emit(event)

        title = f.name if has_file else "(no file selected)"

        self.ctx.context_menu.popup([
            (title, lambda: None, False),
            DIVIDER,
            ("Open", for_file(UIEvent.OPEN_FILE_REQUESTED), has_file),
            ("Extract...", for_file(UIEvent.EXTRACT_FILE_REQUESTED), has_file),
            DIVIDER,
            ("Rename...    F2", for_file(UIEvent.RENAME_REQUESTED), has_file),
            ("Replace with file...", for_file(UIEvent.REPLACE_REQUESTED), has_file),
            ("Delete    Del", for_file(UIEvent.DELETE_REQUESTED), has_file),
            DIVIDER,
            ("Add files...", for_archive(UIEvent.ADD_FILES_REQUESTED), True),
            ("Extract all...", for_archive(UIEvent.EXTRACT_ALL_REQUESTED), has_archive),
        ])

    # --- hand to the OS -------------------------------------------------------

    def _temp_copy(self, f):
        """Write f to this session's temp folder so another program can open it (edits there aren't saved back)."""
        if self._temp_dir is None:
            self._temp_dir = tempfile.mkdtemp(prefix="blitfs-viewer-")
            atexit.register(shutil.rmtree, self._temp_dir, True)

        path = BLITFS.ExtractFile(f, self._temp_dir)

        if path is None:            # unsafe name like ../x: fall back to just the base name
            safe = BLITFS.FSInput(os.path.basename(f.name) or "file", f.bytelist)
            path = BLITFS.ExtractFile(safe, self._temp_dir)

        return path

    def open_default(self, index):
        f = self._file(index)

        if f is None:
            return

        try:
            Shell.open_default(self._temp_copy(f))
        except OSError as e:
            Dialogs.error(f"Couldn't open {f.name}:\n{e}")
            return

        self.ctx.status.set(f"Opened a copy of {f.name} (changes there are not saved into the archive)")

    # --- edit -----------------------------------------------------------------

    def extract(self, index):
        f = self._file(index)

        if f is None:
            return

        dest = Dialogs.choose_save_path(os.path.basename(f.name))

        if not dest:
            return

        try:
            with open(dest, "wb") as out:
                out.write(f.bytelist)
        except OSError as e:
            Dialogs.error(f"Couldn't write {dest}:\n{e}")
            return

        self.ctx.status.set(f"Extracted {f.name} -> {dest}")

    def rename(self, index):
        f = self._file(index)

        if f is None:
            return

        name = Dialogs.ask_text("New name (use / for folders):", f.name)

        if name is None:
            return

        name = name.strip().replace("\\", "/")

        if not name or name == f.name:
            return

        if self.ctx.state.name_taken(name, except_index=index):
            Dialogs.error(f"There is already a file called {name}.")
            return

        f.name = name
        self._changed(index)

    def delete(self, index):
        f = self._file(index)

        if f is None:
            return

        if not Dialogs.confirm(f"Delete {f.name} from the archive?", "Delete"):
            return

        del self.ctx.state.fs.Filesystem[index]
        self._changed(min(index, self.ctx.state.fs.size - 1))

    def replace(self, index):
        f = self._file(index)

        if f is None:
            return

        path = Dialogs.choose_file(f"Replace {f.name} with")

        if not path:
            return

        try:
            with open(path, "rb") as fh:
                f.bytelist = fh.read()
        except OSError as e:
            Dialogs.error(f"Couldn't read {path}:\n{e}")
            return

        self._changed(index)
