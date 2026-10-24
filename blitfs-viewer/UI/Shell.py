"""Hand files to the operating system's default program.

On Windows this never calls ShellExecute (os.startfile) in our own process: with a broken file
association (e.g. .glb still pointing at the uninstalled 3D Viewer) it crashes inside FLTK apps with
an access violation, killing the viewer. Explorer does the launching instead, exactly like a
double-click in Explorer, including its "How do you want to open this file?" picker when needed.
"""
import os
import subprocess
import sys


def open_default(path):
    if sys.platform == "win32":
        subprocess.Popen(["explorer.exe", os.path.normpath(path)])
    elif sys.platform == "darwin":
        subprocess.Popen(["open", path])
    else:
        subprocess.Popen(["xdg-open", path])
