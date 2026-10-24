import fltk as flk

from UI.Events import EventBus, UIEvent


class MenuBar:
    """File menu. Only emits requests; what they do is the controller's business."""

    def __init__(self, x, y, w, h, bus: EventBus):
        self.bus = bus
        self.widget = flk.Fl_Menu_Bar(x, y, w, h)

        self._add("&File/&Open...", flk.FL_CTRL + ord("o"), UIEvent.OPEN_REQUESTED)
        self._add("&File/&Save", flk.FL_CTRL + ord("s"), UIEvent.SAVE_REQUESTED)
        self._add("&File/Save &As...", flk.FL_CTRL + flk.FL_SHIFT + ord("s"), UIEvent.SAVE_AS_REQUESTED, divider=True)

        self._add("&File/A&dd files...", flk.FL_CTRL + ord("d"), UIEvent.ADD_FILES_REQUESTED)
        self._add("&File/Extract &all...", flk.FL_CTRL + flk.FL_SHIFT + ord("e"), UIEvent.EXTRACT_ALL_REQUESTED, divider=True)

        self._add("&File/&Quit", flk.FL_CTRL + ord("q"), UIEvent.QUIT_REQUESTED)

    def _add(self, label, shortcut, event, divider=False):
        flags = flk.FL_MENU_DIVIDER if divider else 0

        self.widget.add(label, shortcut, lambda *_: self.bus.emit(event), None, flags)
