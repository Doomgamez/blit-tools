import fltk as flk


class StatusBar:
    def __init__(self, x, y, w, h):
        self.widget = flk.Fl_Box(x, y, w, h)
        self.widget.box(flk.FL_THIN_DOWN_BOX)
        self.widget.align(flk.FL_ALIGN_LEFT | flk.FL_ALIGN_INSIDE)

        self.set("Ready")

    def set(self, text):
        self.widget.copy_label(" " + text)      # copy_label: FLTK keeps its own copy of the string
