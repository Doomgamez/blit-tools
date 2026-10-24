"""Whole-archive actions: open, save, add files, extract all, quit (with the unsaved-changes guard)."""
import os
import sys

import BLITFS
from UI import Dialogs, Privileges
from UI.Events import UIEvent
from UI.MainWindow.Context import MainWindowContext

TITLE = "BLITFS Viewer" + (" (Administrator)" if Privileges.ELEVATED else "")


class ArchiveActions:
    def __init__(self, ctx: MainWindowContext):
        self.ctx = ctx

        bus = ctx.bus
        bus.on(UIEvent.OPEN_REQUESTED, self.ask_open)
        bus.on(UIEvent.OPEN_PATH, self.open_path)
        bus.on(UIEvent.FILE_DROP, lambda paths: self.open_path(paths[0]))
        bus.on(UIEvent.ARCHIVE_OPENED, lambda state: self.refresh(-1))
        bus.on(UIEvent.ARCHIVE_CHANGED, lambda state, keep: self.refresh(keep))
        bus.on(UIEvent.SAVE_REQUESTED, self.save)
        bus.on(UIEvent.SAVE_AS_REQUESTED, self.save_as)
        bus.on(UIEvent.ADD_FILES_REQUESTED, self.add_files)
        bus.on(UIEvent.EXTRACT_ALL_REQUESTED, self.extract_all)
        bus.on(UIEvent.QUIT_REQUESTED, self.quit)

        ctx.window.callback(lambda *_: self.quit())     # window X button / Escape
        ctx.window.copy_label(TITLE)

        if Privileges.ELEVATED and sys.platform == "win32":
            # Windows blocks drag & drop from a normal Explorer into an elevated window (UIPI)
            ctx.status.set("Running as administrator: dragging files in from Explorer won't work")

    # --- open / close -------------------------------------------------------

    def ask_open(self):
        if not self.can_discard():
            return

        path = Dialogs.choose_archive()

        if path:
            self._load(path)

    def open_path(self, path):
        if self.can_discard():
            self._load(path)

    def _load(self, path):
        try:
            fs = BLITFS.Open(path)
        except (OSError, BLITFS.BlitFSError) as e:
            Dialogs.error(f"Couldn't open {path}:\n{e}")
            return

        state = self.ctx.state
        state.path = path
        state.fs = fs
        state.dirty = False

        self.ctx.bus.emit(UIEvent.ARCHIVE_OPENED, state)

    def quit(self):
        if self.can_discard():
            self.ctx.window.hide()

    def can_discard(self):
        """True when it's fine to throw away the open archive (saved, or the user said so)."""
        if not self.ctx.state.dirty:
            return True

        choice = Dialogs.unsaved_choice(f"Save changes to {self._name()}?")

        if choice == "save":
            return self.save()

        return choice == "discard"

    # --- save ---------------------------------------------------------------

    def save(self):
        if self.ctx.state.fs is None:
            return False

        if self.ctx.state.path is None:
            return self.save_as()

        return self._write(self.ctx.state.path)

    def save_as(self):
        if self.ctx.state.fs is None:
            return False

        path = Dialogs.choose_archive_save_path(self._name())

        if not path:
            return False

        # set the new path first so the title shows the new name after saving
        old_path = self.ctx.state.path
        self.ctx.state.path = path

        if not self._write(path):
            self.ctx.state.path = old_path
            return False

        return True

    def _write(self, path):
        tmp = path + ".tmp"

        try:
            BLITFS.Save(tmp, self.ctx.state.fs)
            os.replace(tmp, path)           # never leave a half-written archive behind
        except OSError as e:
            Dialogs.error(f"Couldn't save {path}:\n{e}")

            if os.path.exists(tmp):
                os.remove(tmp)

            return False

        self.ctx.state.dirty = False
        self.refresh(self.ctx.file_list.selected_index())
        self.ctx.status.set(f"Saved {path}")

        return True

    # --- contents -------------------------------------------------------------

    def add_files(self):
        state = self.ctx.state

        if state.fs is None:                # adding to nothing starts a new, unsaved archive
            state.fs = BLITFS.FS()
            state.path = None
            state.dirty = False

        paths = Dialogs.choose_files()

        if not paths:
            return

        added = sum(1 for p in paths if self._add_one(p))

        if added:
            state.dirty = True
            self.ctx.bus.emit(UIEvent.ARCHIVE_CHANGED, state, state.fs.size - 1)
            self.ctx.status.set(f"Added {added} file(s)")

    def _add_one(self, path):
        """Add or replace one file from disk. Returns True if the archive changed."""
        state = self.ctx.state
        name = os.path.basename(path)

        if state.name_taken(name):
            if not Dialogs.confirm(f"{name} is already in the archive. Replace it?", "Replace"):
                return False

        try:
            with open(path, "rb") as fh:
                data = fh.read()
        except OSError as e:
            Dialogs.error(f"Couldn't read {path}:\n{e}")
            return False

        existing = state.fs.Get(name)

        if existing:
            existing.bytelist = data
        else:
            state.fs.Filesystem.append(BLITFS.FSInput(name, data))

        return True

    def extract_all(self):
        if self.ctx.state.fs is None:
            Dialogs.error("Open an archive first.")
            return

        folder = Dialogs.choose_folder()

        if not folder:
            return

        try:
            written, skipped = BLITFS.Extract(self.ctx.state.fs, folder)
        except OSError as e:
            Dialogs.error(f"Extract failed:\n{e}")
            return

        note = f", skipped {len(skipped)} unsafe name(s)" if skipped else ""
        self.ctx.status.set(f"Extracted {len(written)} file(s) to {folder}{note}")

    # --- view -----------------------------------------------------------------

    def _name(self):
        if self.ctx.state.path:
            return os.path.basename(self.ctx.state.path)

        return "untitled.bfs"

    def refresh(self, keep_selected):
        state = self.ctx.state
        unsaved = " *" if state.dirty else ""

        self.ctx.window.copy_label(f"{TITLE} - {self._name()}{unsaved}")
        self.ctx.file_list.show(state.fs, keep_selected)

        if keep_selected < 0:
            total = sum(f.size for f in state.fs)
            where = state.path or "(not saved yet)"

            self.ctx.status.set(f"{where}  |  {state.fs.size} file(s), {total:,} bytes")
