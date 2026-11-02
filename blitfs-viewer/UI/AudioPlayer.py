"""Plays audio out of an archive through a temp file, the players all want a file."""
import os
import subprocess
import sys
import tempfile


class AudioPlayer:
    """WAV plays inside the viewer on Windows, anything else opens in the system's own player."""

    def __init__(self):
        self.path = None

    def _write(self, data, kind):
        self.cleanup()

        fd, self.path = tempfile.mkstemp(suffix="." + kind)

        with os.fdopen(fd, "wb") as tmp:
            tmp.write(data)

        return self.path

    def plays_here(self, kind):
        return kind == "wav" and sys.platform == "win32"

    def play(self, data, kind):
        path = self._write(data, kind)

        if self.plays_here(kind):
            import winsound

            winsound.PlaySound(path, winsound.SND_FILENAME | winsound.SND_ASYNC)
        elif sys.platform == "win32":
            os.startfile(path)
        else:
            subprocess.Popen(["open" if sys.platform == "darwin" else "xdg-open", path])

    def stop(self):
        if sys.platform == "win32":
            import winsound

            winsound.PlaySound(None, winsound.SND_PURGE)

    def cleanup(self):
        self.stop()

        if self.path:
            try:
                os.remove(self.path)
            except OSError:
                pass                # the system's player can still have it open

            self.path = None
