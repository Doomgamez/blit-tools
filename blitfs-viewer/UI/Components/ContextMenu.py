from typing import Callable

import fltk as flk

DIVIDER = None      # put in an item list to draw a line after the previous item


class ContextMenu:
    """Popup menu at the mouse. Whoever opens it decides the items: [(label, func, enabled), DIVIDER, ...]."""

    def __init__(self):
        self.widget = flk.Fl_Menu_Button(0, 0, 1, 1)
        self.widget.type(flk.Fl_Menu_Button.POPUP3)   # draws nothing; only pops up when told to

    def popup(self, items: list[tuple[str, Callable[[], None], bool] | None]):
        self.widget.clear()

        for item in items:
            if item is DIVIDER:
                self._divide_after_last()
                continue

            label, func, enabled = item

            # '/' makes submenus and '&' underlines in FLTK labels; escape them so names show as-is
            label = label.replace("&", "&&").replace("/", "\\/")
            flags = 0 if enabled else flk.FL_MENU_INACTIVE

            self.widget.add(label, 0, lambda *_, f=func: f(), None, flags)

        self.widget.popup()

    def _divide_after_last(self):
        last = self.widget.size() - 2           # size() counts the end marker

        if last >= 0:
            self.widget.mode(last, self.widget.mode(last) | flk.FL_MENU_DIVIDER)
