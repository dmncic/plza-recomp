"""Lumiose Online's entry script (tools/lumiose/entry.py), run so that this process lasts as long as the game.

entry.py starts the game and returns at once; the AppImage stays mounted only while tools/linux/AppRun's process
does, and the game runs from that mount. So this process becomes the child subreaper of everything it starts (the
game, its model preview helper) and waits for all of them.

The mod's preparation (tools/lumiose/setup.py the first time, auto_prepare.py after an update) takes minutes with no
window of its own - it looked like a crash, and starting again broke it: a progress window (zenity) is shown while it
runs, and its output goes to logs/lumiose-prepare.log. usage: run_entry.py <path of entry.py>
"""
import ctypes
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys

PR_SET_CHILD_SUBREAPER = 36
try:
    ctypes.CDLL(None, use_errno=True).prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0)
except (OSError, AttributeError):
    pass

entry = sys.argv[1]
logs = Path(entry).resolve().parents[2] / 'logs'
_run = subprocess.run


def run(cmd, *args, **kwargs):
    words = ' '.join(map(str, cmd)) if isinstance(cmd, (list, tuple)) else str(cmd)
    if 'setup.py' not in words and 'auto_prepare.py' not in words:
        return _run(cmd, *args, **kwargs)
    first = 'setup.py' in words
    text = ('Preparing the online mod for your game.\nThis takes a few minutes the first time - keep this window open.'
            if first else 'Preparing the downloaded Lumiose Online update.\nThis takes a few minutes - keep this window open.')
    zenity, dialog = shutil.which('zenity'), None
    if zenity:
        try:
            dialog = subprocess.Popen([zenity, '--progress', '--pulsate', '--no-cancel', '--auto-close',
                                       '--title=Lumiose Online', '--text=' + text, '--width=460'],
                                      stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except OSError:
            dialog = None
    log = None
    if 'stdout' not in kwargs and not sys.__stdout__.isatty():
        try:
            logs.mkdir(parents=True, exist_ok=True)
            log = (logs / 'lumiose-prepare.log').open('a', encoding='utf-8')
            kwargs['stdout'], kwargs['stderr'] = log, subprocess.STDOUT
        except OSError:
            log = None
    try:
        return _run(cmd, *args, **kwargs)
    finally:
        if log:
            log.close()
        if dialog:
            try:
                dialog.stdin.write(b'100\n')
                dialog.stdin.close()
            except OSError:
                pass
            dialog.wait()


subprocess.run = run
sys.argv = [entry]
status = 0
try:
    runpy.run_path(entry, run_name='__main__')
except SystemExit as e:
    status = e.code if isinstance(e.code, int) else (0 if e.code is None else 1)
    # (entry.py shows its error with kdialog only: KDE / SteamOS's desktop; elsewhere zenity)
    if status and not shutil.which('kdialog') and shutil.which('zenity'):
        subprocess.Popen([shutil.which('zenity'), '--error', '--title=Lumiose Online',
                          '--text=Lumiose Online could not start.\nDetails: ' + str(logs / 'lumiose-launch.log')],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
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
