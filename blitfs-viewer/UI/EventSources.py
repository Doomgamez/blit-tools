"""Translate raw FLTK events into UIEvents. The only module that knows FLTK event codes.

Each source takes the FLTK event and returns (consumed, emitted):
consumed -> the window returns 1 for it; emitted -> (UIEvent, *args) to send on the bus, or None.
New event? Write a function and add it to SOURCES.
"""
from urllib.parse import unquote, urlparse

import fltk as flk

from UI.Events import UIEvent


def _drop_path(line):
    # Windows gives plain paths; Linux/macOS file managers give file:// URLs
    if line.startswith("file://"):
        return unquote(urlparse(line).path)

    return line


def file_drop(event):
    if event in (flk.FL_DND_ENTER, flk.FL_DND_DRAG, flk.FL_DND_RELEASE):
        return True, None                   # accept the drag so FLTK sends FL_PASTE on release

    if event == flk.FL_PASTE:
        lines = flk.Fl.event_text().splitlines()
        paths = [_drop_path(p.strip()) for p in lines if p.strip()]

        if not paths:
            return True, None

        return True, (UIEvent.FILE_DROP, paths)

    return False, None


SOURCES = [file_drop]
