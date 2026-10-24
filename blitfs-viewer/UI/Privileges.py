"""Is the viewer running elevated (Windows Administrator, or root / sudo elsewhere)?"""
import ctypes
import os
import sys


def is_elevated():
    if sys.platform == "win32":
        try:
            return bool(ctypes.windll.shell32.IsUserAnAdmin())     # true only with an elevated token
        except (AttributeError, OSError):
            return False

    return hasattr(os, "geteuid") and os.geteuid() == 0         # root, which is what sudo gives you


ELEVATED = is_elevated()        # can't change while the process runs, so check once
