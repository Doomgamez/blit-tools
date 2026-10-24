import fltk as flk


class TextView:
    """Read-only monospace text, used for text files and hex dumps."""

    def __init__(self, x, y, w, h):
        self.buffer = flk.Fl_Text_Buffer()      # keep a reference: the display doesn't own it

        self.widget = flk.Fl_Text_Display(x, y, w, h)
        self.widget.buffer(self.buffer)
        self.widget.textfont(flk.FL_COURIER)

    def set_text(self, text):
        self.buffer.text(text)
        self.widget.scroll(0, 0)

    def show(self):
        self.widget.show()

    def hide(self):
        self.widget.hide()
