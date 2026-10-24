import os
import tempfile

import fltk as flk

_LOADERS = {
    "png": flk.Fl_PNG_Image,
    "jpeg": flk.Fl_JPEG_Image,
    "gif": flk.Fl_GIF_Image,
    "bmp": flk.Fl_BMP_Image,
}


class ImageView:
    """Shows an image from raw bytes, scaled down to fit (never up)."""

    def __init__(self, x, y, w, h):
        self.widget = flk.Fl_Box(x, y, w, h)
        self.widget.box(flk.FL_DOWN_BOX)
        self.widget.color(flk.FL_DARK3)

        self._images = []                       # FLTK doesn't own images; keep them alive

    def set_image(self, data, kind):
        """Returns False when FLTK can't decode it, so the caller can fall back to hex."""
        img = self._load(data, kind)

        if img.fail() or img.w() <= 0 or img.h() <= 0:
            return False

        shown = self._fit(img)

        self._images = [img, shown]
        self.widget.image(shown)
        self.widget.label(None)
        self.widget.redraw()

        return True

    def _load(self, data, kind):
        # the image loaders take a filename, so go through a temp file
        fd, path = tempfile.mkstemp(suffix="." + kind)

        try:
            with os.fdopen(fd, "wb") as tmp:
                tmp.write(data)

            return _LOADERS[kind](path)
        finally:
            os.remove(path)

    def _fit(self, img):
        box_w = self.widget.w() - 8
        box_h = self.widget.h() - 8
        scale = min(1.0, box_w / img.w(), box_h / img.h())

        if scale >= 1:
            return img

        return img.copy(max(1, int(img.w() * scale)), max(1, int(img.h() * scale)))

    def clear(self):
        self.widget.image(None)
        self._images = []

    def show(self):
        self.widget.show()

    def hide(self):
        self.widget.hide()
