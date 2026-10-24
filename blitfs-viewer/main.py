import faulthandler
import sys

from UI.Events import UIEvent
from UI.Window import Window
from UI.MainWindow.LoadElements import LoadWindowElements

def main(argv):
    faulthandler.enable()

    window = Window(LoadWindowElements)

    if len(argv) > 1:       # main.py game.bfs  (also what dropping an archive on main.py does)
        path = argv[1]
        window.on_start(lambda: window.events.emit(UIEvent.OPEN_PATH, path))

    return window.run()

if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
