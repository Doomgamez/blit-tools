import fltk as flk

from UI import AudioInfo
from UI.AudioPlayer import AudioPlayer

BUTTON_W = 110
BUTTON_H = 28
MARGIN = 10


class AudioView:
    """Shows what an audio file is, with buttons to listen to it."""

    def __init__(self, x, y, w, h):
        self.player = AudioPlayer()
        self.data = b""
        self.kind = None

        self.group = flk.Fl_Group(x, y, w, h)
        self.group.box(flk.FL_DOWN_BOX)

        self.play_button = flk.Fl_Button(x + MARGIN, y + MARGIN, BUTTON_W, BUTTON_H, "Play")
        self.stop_button = flk.Fl_Button(x + MARGIN * 2 + BUTTON_W, y + MARGIN, BUTTON_W, BUTTON_H, "Stop")

        self.details = flk.Fl_Box(x + MARGIN, y + MARGIN * 2 + BUTTON_H, w - MARGIN * 2, h - MARGIN * 3 - BUTTON_H)
        self.details.align(flk.FL_ALIGN_TOP_LEFT | flk.FL_ALIGN_INSIDE | flk.FL_ALIGN_WRAP)
        self.details.labelsize(15)

        self.group.end()
        self.group.resizable(self.details)

        self.play_button.callback(self._play)
        self.stop_button.callback(self._stop)

    def set_audio(self, data, kind):
        self.player.cleanup()
        self.data = data
        self.kind = kind

        lines = AudioInfo.describe(data, kind)

        if not self.player.plays_here(kind):
            lines.append("")
            lines.append("Play opens it in your system's audio player.")

        self.details.copy_label("\n".join(lines))

        if self.player.plays_here(kind):
            self.stop_button.activate()
        else:
            self.stop_button.deactivate()

        self.group.redraw()

    def clear(self):
        self.player.cleanup()
        self.data = b""
        self.kind = None

    def show(self):
        self.group.show()

    def hide(self):
        self.group.hide()

    def _play(self, _widget):
        if self.kind:
            try:
                self.player.play(self.data, self.kind)
            except OSError as e:
                flk.fl_alert(f"Could not play this file: {e}")

    def _stop(self, _widget):
        self.player.stop()
