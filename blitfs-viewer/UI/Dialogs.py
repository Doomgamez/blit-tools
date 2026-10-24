"""Native pickers and popups. Each returns None when the user cancels."""
import fltk as flk

ARCHIVE_FILTER = "BLITFS archives\t*.{bfs,blfs,blitfs}\nAll files\t*"


def _chooser(kind, title, filter_=None, preset=None):
    chooser = flk.Fl_Native_File_Chooser()
    chooser.type(kind)
    chooser.title(title)

    if filter_:
        chooser.filter(filter_)

    if preset:
        chooser.preset_file(preset)

    if kind == flk.Fl_Native_File_Chooser.BROWSE_SAVE_FILE:
        chooser.options(flk.Fl_Native_File_Chooser.SAVEAS_CONFIRM)

    if chooser.show() != 0:             # 0 = picked, 1 = cancelled, -1 = error
        return None

    return chooser


def _one(*args, **kw):
    chooser = _chooser(*args, **kw)

    return chooser.filename() if chooser else None


def choose_archive():
    return _one(flk.Fl_Native_File_Chooser.BROWSE_FILE, "Open BLITFS archive", ARCHIVE_FILTER)


def choose_archive_save_path(suggested_name=None):
    return _one(flk.Fl_Native_File_Chooser.BROWSE_SAVE_FILE, "Save BLITFS archive as", ARCHIVE_FILTER, suggested_name)


def choose_file(title="Choose a file"):
    return _one(flk.Fl_Native_File_Chooser.BROWSE_FILE, title)


def choose_files(title="Add files"):
    chooser = _chooser(flk.Fl_Native_File_Chooser.BROWSE_MULTI_FILE, title)

    if not chooser:
        return None

    return [chooser.filename(i) for i in range(chooser.count())]


def choose_folder(title="Extract to folder"):
    return _one(flk.Fl_Native_File_Chooser.BROWSE_DIRECTORY, title)


def choose_save_path(suggested_name):
    return _one(flk.Fl_Native_File_Chooser.BROWSE_SAVE_FILE, "Extract file as", preset=suggested_name)


def ask_text(prompt, default=""):
    return flk.fl_input(prompt, default)


def confirm(question, yes="Yes", no="Cancel"):
    return flk.fl_choice(question, no, yes, None) == 1


def unsaved_choice(question):
    """'save', 'discard' or None (cancel)."""
    picked = flk.fl_choice(question, "Cancel", "Don't save", "Save")

    return {0: None, 1: "discard", 2: "save"}[picked]


def error(message):
    flk.fl_alert(message)
