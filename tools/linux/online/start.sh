#!/bin/bash
# PLAY ONLINE (the launcher's button; tools/linux/AppRun starts this when the launcher exits with 77): Lumiose Online's
# own launcher (tools/linux/build_online.sh) in <data root>/online - a folder of its own for its game code, its mod
# builds, its save and settings (the mod wants a new save: the offline one is never touched). The game data is the
# offline setup's: exefs copied (the online game code is built from it), the 6 GB data image and the DLC linked.
set -u
O=$(dirname "$(readlink -f "$0")")  # <AppImage mount>/opt/zarecomp/online
APP=$(dirname "$O")
DATA=${LARECOMP_DATA_ROOT:?}
R=$DATA/online
X=$DATA/extracted
PY=$APP/python/bin/python3
NAME="PL:Z-A Recomp - Lumiose Online"
msg() {
    echo "$NAME: $1" >&2
    if command -v zenity >/dev/null 2>&1; then zenity --error --title="$NAME" --text="$1" 2>/dev/null
    elif command -v kdialog >/dev/null 2>&1; then kdialog --title "$NAME" --error "$1" 2>/dev/null
    fi
}

[ -f "$X/exefs/main" ] && [ -f "$X/romfs.bin" ] || { msg "Set the game up first (GAME FILES in the launcher), then press PLAY ONLINE."; exit 1; }
mkdir -p "$R/extracted/aoc" "$R/lumiose" "$R/logs" || { msg "Can't write to $R."; exit 1; }

# The first start builds the online game code and prepares the mod: about 8 GB while it compiles
if [ ! -f "$R/extracted/lasgame.so" ]; then
    free=$(df -Pk "$R" 2>/dev/null | awk 'NR == 2 {print int($4 / 1048576)}')
    if [ -n "$free" ] && [ "$free" -lt 8 ]; then
        msg "Lumiose Online needs about 8 GB free for its own game code the first time; $free GB free in $R."
        exit 1
    fi
fi

# Its scripts take their folder from their own path: a copy here, refreshed with each new AppImage
if ! cmp -s "$O/VERSION" "$R/.bundle-version"; then
    rm -rf "$R/tools" && cp -a "$O/tools" "$R/tools" && cp -f "$O/VERSION" "$R/.bundle-version" ||
        { msg "Can't copy Lumiose Online's scripts into $R."; exit 1; }
fi
[ -f "$R/lumiose/subsdk9" ] || cp "$O/lumiose/subsdk9" "$R/lumiose/subsdk9"
# The program, the compiler and the runtime's headers: links into this AppImage (mounted somewhere else each start)
for n in lasrecomp zig runtime; do ln -sfn "$O/$n" "$R/$n"; done

# The game files: exefs copied when the offline setup's changed (the online game code is then built again)
if ! cmp -s "$X/exefs/main" "$R/extracted/exefs/main" 2>/dev/null; then
    rm -rf "$R/extracted/exefs" && cp -a "$X/exefs" "$R/extracted/exefs" || { msg "Can't copy the game's exefs into $R."; exit 1; }
fi
ln -sfn ../../extracted/romfs.bin "$R/extracted/romfs.bin"
# (the Mega Dimension DLC: its RomFS image under its title id, where that runtime looks - runtime/services/misc.c)
if [ -f "$X/dlc.bin" ]; then ln -sfn ../../../extracted/dlc.bin "$R/extracted/aoc/0100f43008c45002.bin"
else rm -f "$R/extracted/aoc/0100f43008c45002.bin"; fi

cd "$R" || exit 1
export LARECOMP_DATA_ROOT="$R"     # (never that runtime's default, ~/.local/share/zarecomp: an older offline setup's)
export LARECOMP_NO_UPDATE_CHECK=1  # (its launcher's own release download: the AppImage is updated as a whole)
unset PYTHONHOME PYTHONPATH
# (through run_entry.py: it waits for the game, which entry.py leaves running - the AppImage must stay mounted)
exec "$PY" "$O/run_entry.py" "$R/tools/lumiose/entry.py"
