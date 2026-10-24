import fltk as flk

import BLITFS
from UI.Events import EventBus, UIEvent
from UI.Preview import human_size

KEY_EVENTS = {
    flk.FL_Delete: UIEvent.DELETE_REQUESTED,
    flk.FL_F + 2: UIEvent.RENAME_REQUESTED,
    flk.FL_Enter: UIEvent.FILE_ACTIVATED,
    flk.FL_KP_Enter: UIEvent.FILE_ACTIVATED,
}


class _Browser(flk.Fl_Hold_Browser):
    """Fl_Hold_Browser that also reports right-clicks and the Delete / F2 / Enter keys."""

    def __init__(self, x, y, w, h, owner):
        super().__init__(x, y, w, h)
        self.owner = owner

    def handle(self, event):
        if event == flk.FL_PUSH and flk.Fl.event_button() == flk.FL_RIGHT_MOUSE:
            super().handle(event)                       # let the browser select the line under the mouse
            self.owner.emit(UIEvent.CONTEXT_MENU_REQUESTED)

            return 1

        if event == flk.FL_KEYBOARD:
            key_event = KEY_EVENTS.get(flk.Fl.event_key())

            if key_event:
                self.owner.emit(key_event)

                return 1

        return super().handle(event)


class FileList:
    """Archive contents, one line per file. Emits FILE_SELECTED / FILE_ACTIVATED / CONTEXT_MENU_REQUESTED etc.
    with the 0-based index (-1 when nothing is selected)."""

    def __init__(self, x, y, w, h, bus: EventBus):
        self.bus = bus

        self.widget = _Browser(x, y, w, h, self)
        self.widget.textfont(flk.FL_COURIER)
        self.widget.when(flk.FL_WHEN_CHANGED | flk.FL_WHEN_NOT_CHANGED)   # so a double-click on the same line still calls back
        self.widget.callback(self._on_click)

    def show(self, fs: BLITFS.FS, select=-1):
        self.widget.clear()

        for f in fs:
            # "@." stops FLTK reading @-codes, so names starting with '@' show as-is
            self.widget.add(f"@.{human_size(f.size):>10}  {f.name}")

        if 0 <= select < fs.size:
            self.widget.value(select + 1)
            self.widget.middleline(select + 1)      # scroll it into view

        self.emit(UIEvent.FILE_SELECTED)

    def selected_index(self):
        return self.widget.value() - 1          # FLTK lines are 1-based, 0 = nothing

    def emit(self, event):
        self.bus.emit(event, self.selected_index())

    def _on_click(self, *_):
        double_click = (
            flk.Fl.event_clicks()
            and flk.Fl.event_button() == flk.FL_LEFT_MOUSE
            and flk.Fl.event() == flk.FL_RELEASE
        )

        if double_click:
            flk.Fl.event_clicks(0)              # don't count a third click as another double-click
            self.emit(UIEvent.FILE_ACTIVATED)
        else:
            self.emit(UIEvent.FILE_SELECTED)
