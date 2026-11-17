"""Lumiose Online's entry script (tools/lumiose/entry.py), run so that this process lasts as long as the game.

entry.py starts the game and returns at once; the AppImage stays mounted only while tools/linux/AppRun's process
does, and the game runs from that mount. So this process becomes the child subreaper of everything it starts (the
game, its model preview helper) and waits for all of them. usage: run_entry.py <path of entry.py>
"""
import ctypes
import os
import runpy
import sys

PR_SET_CHILD_SUBREAPER = 36
try:
    ctypes.CDLL(None, use_errno=True).prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0)
except (OSError, AttributeError):
    pass
entry = sys.argv[1]
sys.argv = [entry]
status = 0
try:
    runpy.run_path(entry, run_name='__main__')
except SystemExit as e:
    status = e.code if isinstance(e.code, int) else (0 if e.code is None else 1)
finally:
    sys.stdout.flush()
    while True:  # (the game and anything else started, re-parented here when their parent left)
        try:
            os.wait()
        except ChildProcessError:
            break
        except InterruptedError:
            continue
sys.exit(status)
