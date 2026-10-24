"""Layout for the main window: places the components and hooks up the controllers."""
import fltk as flk

from UI.Components.ContextMenu import ContextMenu
from UI.Components.FileList import FileList
from UI.Components.MenuBar import MenuBar
from UI.Components.PreviewPane import PreviewPane
from UI.Components.StatusBar import StatusBar
from UI.MainWindow.ArchiveActions import ArchiveActions
from UI.MainWindow.Context import MainWindowContext
from UI.MainWindow.FileActions import FileActions

MENU_H = 25
STATUS_H = 22
LIST_W = 320


def LoadWindowElements(app):
    win = app.window
    w, h = win.w(), win.h()

    body_y = MENU_H
    body_h = h - MENU_H - STATUS_H

    MenuBar(0, 0, w, MENU_H, app.events)

    # Fl_Tile: drag the border between the list and the preview to resize them
    tile = flk.Fl_Tile(0, body_y, w, body_h)
    file_list = FileList(0, body_y, LIST_W, body_h, app.events)
    preview = PreviewPane(LIST_W, body_y, w - LIST_W, body_h)
    tile.end()

    status = StatusBar(0, h - STATUS_H, w, STATUS_H)
    context_menu = ContextMenu()

    win.resizable(tile)

    # Python must keep the component and controller objects alive; FLTK only owns the widgets
    app.context = MainWindowContext(app.events, win, file_list, preview, status, context_menu)
    app.controllers = [ArchiveActions(app.context), FileActions(app.context)]
