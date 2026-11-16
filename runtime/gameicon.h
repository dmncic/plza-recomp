// The game's own icon (runtime/gameicon.c), taken from the player's own game files when they are set up - never shipped:
// <data root>/icon.png, used for the window / title bar, the start screen's header and the application menu entry.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The icon's path (<data root>/icon.png) in out; false = none yet.
bool gameicon_path(char *out, size_t n);
// The game's icon (its JPEG, icon_<language>.dat) decoded and written as icon.png; an installed menu entry's icon
// (--install: ~/.local/share/icons/hicolor/256x256/apps/zarecomp.png) replaced by it too.
bool gameicon_save(const uint8_t *jpeg, size_t n);
// A dumped title folder's icon: icon_AmericanEnglish.dat (or another icon_*.dat) in it, in its control/ or beside it in
// ../control/ (the control content dumped next to exefs / romfs). False: none found.
bool gameicon_from_folder(const char *folder);
// In the background, when there is no icon yet: read from the base game's package (with these keys, NULL / "": searched).
// Each package path is tried once.
void gameicon_fetch_start(const char *base_pkg, const char *keys);
// Changes each time icon.png is written (the window icon and the header's image are reloaded then).
unsigned gameicon_generation(void);

#ifdef __cplusplus
}
#endif
