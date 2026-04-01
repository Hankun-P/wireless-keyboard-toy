import os
import sys

def resource_path(path):
    if hasattr(sys, "_MEIPASS"):
        base = sys._MEIPASS
    else:
        base = os.path.dirname(__file__)

    return os.path.join(base, "..", path)