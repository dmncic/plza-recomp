// The game's own icon from the player's own game files (runtime/gameicon.h): the control content's icon_*.dat (a
// 256x256 JPEG) decoded with stb_image and written as <data root>/icon.png. Nothing of the game is shipped with the app:
// until the game is set up the app's own icon (docs/icon) is used.
#include "gameicon.h"
#include <SDL3/SDL.h>
#include <dirent.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>
#include "hle.h"
#include "nxpkg.h"
#include "paths.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS 4096
#include "stb/stb_image.h"

static _Atomic unsigned g_gen = 1;
unsigned gameicon_generation(void) { return g_gen; }

static void icon_file(char *out, size_t n) { snprintf(out, n, "%s/icon.png", rt_data_root()); }

bool gameicon_path(char *out, size_t n) {
    struct stat st;
    icon_file(out, n);
    return stat(out, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

// An installed menu entry (tools/linux/AppRun --install) shows the game's icon too: its icon file replaced, the icon
// theme folder touched (desktops re-read it when its time changes).
static void menu_icon(const char *png) {
#if defined(__linux__)
    char base[1024], p[1200], tmp[1300];
    const char *h = getenv("HOME");  // (where --install puts them: ~/.local/share)
    if (!h || !h[0]) return;
    snprintf(base, sizeof base, "%s/.local/share", h);
    struct stat st;
    snprintf(p, sizeof p, "%s/applications/zarecomp.desktop", base);
    if (stat(p, &st) != 0) return;  // (no menu entry: nothing to change)
    snprintf(p, sizeof p, "%s/icons/hicolor/256x256/apps/zarecomp.png", base);
    snprintf(tmp, sizeof tmp, "%s.tmp", p);
    FILE *i = fopen(png, "rb"), *o = i ? fopen(tmp, "wb") : NULL;
    bool ok = i && o;
    char buf[65536];
    size_t k;
    while (ok && (k = fread(buf, 1, sizeof buf, i)) > 0) ok = fwrite(buf, 1, k, o) == k;
    if (i) fclose(i);
    if (o && fclose(o)) ok = false;
    if (!ok || rename(tmp, p) != 0) { remove(tmp); LOG("[icon] the menu entry's icon couldn't be replaced (%s)\n", p); return; }
    snprintf(p, sizeof p, "%s/icons/hicolor", base);
    utime(p, NULL);
    LOG("[icon] the menu entry shows the game's icon\n");
#else
    (void)png;
#endif
}

bool gameicon_save(const uint8_t *jpeg, size_t n) {
    int w, h, c;
    if (!jpeg || n < 4 || n > (1u << 22)) return false;
    stbi_uc *px = stbi_load_from_memory(jpeg, (int)n, &w, &h, &c, 4);
    if (!px) { LOG("[icon] the game's icon couldn't be read: %s\n", stbi_failure_reason()); return false; }
    char path[1100], tmp[1200];
    icon_file(path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    SDL_Surface *s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, px, w * 4);
    bool ok = s && SDL_SavePNG(s, tmp) && rename(tmp, path) == 0;
    if (!ok) LOG("[icon] %s couldn't be written: %s\n", path, SDL_GetError()), remove(tmp);
    SDL_DestroySurface(s);
    stbi_image_free(px);
    if (!ok) return false;
    LOG("[icon] the game's icon (%dx%d) written to %s\n", w, h, path);
    g_gen++;
    menu_icon(path);
    return true;
}

// icon_AmericanEnglish.dat in dir, else any icon_*.dat: its bytes (malloc'd)
static uint8_t *icon_in(const char *dir, size_t *n) {
    char p[1300], pick[300] = "";
    snprintf(p, sizeof p, "%s/icon_AmericanEnglish.dat", dir);
    struct stat st;
    if (stat(p, &st) != 0) {
        DIR *d = opendir(dir);
        if (!d) return NULL;
        struct dirent *e;
        while (!pick[0] && (e = readdir(d))) {
            size_t l = strlen(e->d_name);
            if (l > 9 && l < sizeof pick && !strncmp(e->d_name, "icon_", 5) && !strcmp(e->d_name + l - 4, ".dat"))
                snprintf(pick, sizeof pick, "%s", e->d_name);
        }
        closedir(d);
        if (!pick[0]) return NULL;
        snprintf(p, sizeof p, "%s/%s", dir, pick);
        if (stat(p, &st) != 0) return NULL;
    }
    if (st.st_size <= 0 || st.st_size > (1 << 22)) return NULL;
    FILE *f = fopen(p, "rb");
    uint8_t *b = f ? malloc((size_t)st.st_size) : NULL;
    bool ok = b && fread(b, 1, (size_t)st.st_size, f) == (size_t)st.st_size;
    if (f) fclose(f);
    if (!ok) { free(b); return NULL; }
    *n = (size_t)st.st_size;
    return b;
}

bool gameicon_from_folder(const char *folder) {
    if (!folder || !folder[0]) return false;
    char d[1200];
    const char *sub[] = {"", "/control", "/../control"};
    for (int i = 0; i < 3; i++) {
        snprintf(d, sizeof d, "%s%s", folder, sub[i]);
        size_t n;
        uint8_t *b = icon_in(d, &n);
        if (!b) continue;
        bool ok = gameicon_save(b, n);
        free(b);
        if (ok) return true;
    }
    return false;
}

// ---- in the background, from the base game's package (a game set up before the icon was taken)
static char g_fetch_pkg[1024], g_fetch_keys[1024], g_tried[1024];
static _Atomic bool g_fetching;

static int fetch_thread(void *arg) {
    (void)arg;
    static const uint64_t titles[] = {0x0100F43008C44000ull, 0x01001F5010DFA000ull, 0x0100187003A36000ull, 0x010003F003A34000ull, 0};
    const char *list[1] = {g_fetch_pkg};
    int err;
    char why[512] = "";
    NxPkg *p = nxpkg_open_list(list, 1, g_fetch_keys, titles, &err, why, sizeof why);
    size_t n = 0;
    uint8_t *b = p ? nxpkg_read_icon(p, &n, why, sizeof why) : NULL;
    if (b) gameicon_save(b, n);
    else LOG("[icon] none from %s: %s\n", g_fetch_pkg, why);
    free(b);
    nxpkg_close(p);
    g_fetching = false;
    return 0;
}

void gameicon_fetch_start(const char *base_pkg, const char *keys) {
    char have[1100];
    if (!base_pkg || !base_pkg[0] || g_fetching || !strcmp(base_pkg, g_tried) || gameicon_path(have, sizeof have)) return;
    struct stat st;
    if (stat(base_pkg, &st) != 0 || !S_ISREG(st.st_mode)) return;
    snprintf(g_tried, sizeof g_tried, "%s", base_pkg);
    snprintf(g_fetch_pkg, sizeof g_fetch_pkg, "%s", base_pkg);
    snprintf(g_fetch_keys, sizeof g_fetch_keys, "%s", keys ? keys : "");
    g_fetching = true;
    SDL_Thread *t = SDL_CreateThread(fetch_thread, "gameicon", NULL);
    if (t) SDL_DetachThread(t);
    else g_fetching = false;
}
