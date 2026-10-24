import fltk as flk

import BLITFS
from UI import Preview
from UI.Components.ImageView import ImageView
from UI.Components.TextView import TextView

EMPTY_MESSAGE = "Open a BLITFS archive (File > Open, or drag it onto the window)."


class PreviewPane:
    """Shows a file as an image, text, or hex dump. Both views overlap; one is visible at a time."""

    def __init__(self, x, y, w, h):
        self.group = flk.Fl_Group(x, y, w, h)
        self.text = TextView(x, y, w, h)
        self.image = ImageView(x, y, w, h)
        self.group.end()

        self.group.resizable(self.text.widget)
        self.clear()

    def show_file(self, f: BLITFS.FSInput):
        """Returns how it was shown: 'image', 'text' or 'hex'."""
        kind = Preview.image_kind(f.bytelist)

        if kind and self.image.set_image(f.bytelist, kind):
            self.text.hide()
            self.image.show()

            return "image"

        text = Preview.as_text(f.bytelist)

        if text is not None:
            self._show_text(text)

            return "text"

        self._show_text(Preview.hexdump(f.bytelist))

        return "hex"

    def clear(self, message=None):
        self._show_text(message or EMPTY_MESSAGE)

    def _show_text(self, text):
        self.image.clear()
        self.image.hide()

        self.text.show()
        self.text.set_text(text)
