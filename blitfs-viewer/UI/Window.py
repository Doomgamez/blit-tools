import fltk as flk
from typing import Callable

from UI.Events import EventBus
from UI.EventSources import SOURCES


class _FltkWindow(flk.Fl_Window):
    """Fl_Window that feeds its events through EventSources onto the bus."""

    def __init__(self, w, h, title, bus: EventBus):
        super().__init__(w, h, title)
        self.bus = bus

    def handle(self, event):
        for source in SOURCES:
            consumed, emitted = source(event)

            if emitted:
                self.bus.emit(*emitted)

            if consumed:
                if event in (flk.FL_DND_ENTER, flk.FL_DND_DRAG):
                    # FLTK only delivers FL_DND_RELEASE / FL_PASTE to Fl::belowmouse(), which normally
                    # gets set when a *child* accepts the drag. We accepted it ourselves, so claim it,
                    # or the drop is silently rejected.
                    flk.Fl.belowmouse(self)

                return 1

        return super().handle(event)


class Window:

    LoadElements = Callable[["Window"], None]

    def __init__(self, load_elements: LoadElements, w=900, h=600, title="BLITFS Viewer"):
        self.events = EventBus()
        self.load_elements = load_elements

        self.window = _FltkWindow(w, h, title, self.events)
        self.window.end()

        self._on_start = []

    def on_start(self, func):
        """Run func once the elements are loaded and the window is up (e.g. open a file from argv)."""
        self._on_start.append(func)

    def run(self):
        self.window.begin()
        self.load_elements(self)
        self.window.end()

        self.window.show()

        for func in self._on_start:
            func()

        return flk.Fl.run()
