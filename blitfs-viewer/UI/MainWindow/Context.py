from dataclasses import dataclass, field

import fltk as flk

from UI.Components.ContextMenu import ContextMenu
from UI.Components.FileList import FileList
from UI.Components.PreviewPane import PreviewPane
from UI.Components.StatusBar import StatusBar
from UI.Data import ViewerState
from UI.Events import EventBus


@dataclass
class MainWindowContext:
    """Everything the main-window controllers share."""
    bus: EventBus
    window: flk.Fl_Window
    file_list: FileList
    preview: PreviewPane
    status: StatusBar
    context_menu: ContextMenu
    state: ViewerState = field(default_factory=ViewerState)
