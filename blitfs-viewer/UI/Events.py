from collections import defaultdict
from enum import Enum, auto
from typing import Callable


class UIEvent(Enum):
    # archive level
    FILE_DROP = auto()              # list[str] of dropped paths
    OPEN_REQUESTED = auto()         # user wants to pick an archive
    OPEN_PATH = auto()              # str: open this archive
    ARCHIVE_OPENED = auto()         # ViewerState after a successful open
    ARCHIVE_CHANGED = auto()        # ViewerState after rename/delete/add/replace/save; int: index to keep selected
    SAVE_REQUESTED = auto()
    SAVE_AS_REQUESTED = auto()
    ADD_FILES_REQUESTED = auto()
    EXTRACT_ALL_REQUESTED = auto()
    QUIT_REQUESTED = auto()

    # file level (int: 0-based index in the archive, -1 for none)
    FILE_SELECTED = auto()
    FILE_ACTIVATED = auto()         # double-click / Enter
    CONTEXT_MENU_REQUESTED = auto()
    OPEN_FILE_REQUESTED = auto()
    EXTRACT_FILE_REQUESTED = auto()
    RENAME_REQUESTED = auto()
    DELETE_REQUESTED = auto()
    REPLACE_REQUESTED = auto()


Handler = Callable[..., None]


class EventBus:
    def __init__(self):
        self._handlers: dict[UIEvent, list[Handler]] = defaultdict(list)

    def on(self, event: UIEvent, handler: Handler):
        self._handlers[event].append(handler)

    def emit(self, event: UIEvent, *args):
        for handler in self._handlers[event]:
            handler(*args)
