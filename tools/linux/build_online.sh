#!/bin/bash
# tools/linux/build_online.sh <out dir>: Lumiose Online for the AppImage's PLAY ONLINE button (tools/linux/online/start.sh).
#
# Lumiose Online is a community multiplayer mod made for another recompilation of the game - ZA Recomp - in its fork
# TheMagnumChad/PokeZ-ARecomp-Online (MIT; the mod itself, lumiose/subsdk9, is its authors' Switch module). That fork's
# runtime is built here at a pinned commit with this AppImage's own toolchain (zig cc for glibc 2.31) and libraries
# (SDL 3, FFmpeg, shaderc: the same versions it pins), and put with what its Lumiose scripts use into <out dir>
# (AppDir/opt/zarecomp/online). Not included: its .NET importer (the game files come from this AppImage's own setup),
# its button-icon helper (zaprompts, GPLv3 ooz) and its launcher self-update (the AppImage is updated as a whole).
# Called by tools/linux/make_appimage.sh; ONLINE=0 there leaves it out.
set -euo pipefail
OUT=${1:?usage: build_online.sh <out dir>}
R=$(cd "$(dirname "$0")/../.." && pwd)
L=${LZA_LINUXBUILD:?LZA_LINUXBUILD not set (tools/linux/build_appimage.sh sets it)}
REPO=https://github.com/TheMagnumChad/PokeZ-ARecomp-Online.git
REV=f6b5d53b4a0dea93b9454ec5779e2b0f802c29bb  # (launcher v0.1.38, mod 1.5.8 build 8 bundled)
S=$L/online-src

if [ ! -d "$S/.git" ]; then rm -rf "$S"; git clone -q --filter=blob:none "$REPO" "$S"; fi
if [ "$(git -C "$S" rev-parse HEAD)" != "$REV" ]; then
    git -C "$S" fetch -q origin "$REV" && git -C "$S" checkout -q --detach "$REV"
fi

# the runtime: zig's compiler runtime has no __cpu_model (clang's __builtin_cpu_supports): cpu_builtins.h answers it;
# VMA 3.3.0's header is this repository's (the fork expects it in its deps prefix)
(cd "$S" && PATH="$L/bin:$PATH" ZCC_EXTRA="-I$R/third_party/vma -include $R/tools/linux/online/cpu_builtins.h" \
    make -j"$(nproc)" B=build/zig CC=zcc CXX=zcxx DEPS="$L/prefix" SHADERC=-lshaderc_shared \
    GENOPT="-O1 -DGUEST_VOLATILE" build/zig/lasrecomp) > "$L/make-online.log" 2>&1 ||
    { tail -25 "$L/make-online.log"; echo "build_online: the Lumiose Online runtime didn't build ($L/make-online.log)"; exit 1; }

rm -rf "$OUT" && mkdir -p "$OUT"/{gamelib-src,lasgen,lumiose,tools/rando,tools/lumiose,licenses,docs}
cp "$S/build/zig/lasrecomp" "$OUT/"
strip --strip-debug "$OUT/lasrecomp"
patchelf --set-rpath '$ORIGIN/../lib' "$OUT/lasrecomp"  # (this AppImage's SDL 3 / FFmpeg / shaderc)
ln -s ../zig "$OUT/zig"
# what its game code build includes, and the generator (tools/gen.py, with runtime/game_addresses beside tools/)
cp "$S"/runtime/{cpu.h,simd.h,simd_fp.h,gamelib.h} "$S/tools/gamelib_entry.c" "$S/runtime/hle/hle_symbols.txt" "$OUT/gamelib-src/"
cp -a "$S/runtime" "$OUT/runtime"
cp "$S"/tools/*.py "$S/tools/gamelib_entry.c" "$S/tools/lumiose_test_entry.c" "$OUT/tools/"
cp "$S/tools/rando/patch_variants.json" "$OUT/tools/rando/"
cp "$S"/tools/lumiose/*.py "$S/tools/lumiose/prepackaged.subsdk9" "$OUT/tools/lumiose/"
cp "$S/lumiose/subsdk9" "$OUT/lumiose/"
cat > "$OUT/lasgen/lasgen" <<'EOF'
#!/bin/sh
# The game code generator (tools/gen.py) run by this AppImage's Python - what the fork's frozen lasgen helper is.
d=$(dirname "$(readlink -f "$0")")
exec "$d/../../python/bin/python3" "$d/../tools/gen.py" "$@"
EOF
chmod +x "$OUT/lasgen/lasgen"
# its start screen's files (fonts: OFL / Bitstream Vera; the window icon: this AppImage's)
F=$S/third_party/fonts
cp "$F/Baloo2/Baloo2-SemiBold.ttf" "$F/BIZUDPGothic/BIZUDPGothic-Regular.ttf" "$F/DejaVu/DejaVuSans.ttf" \
   "$F/DejaVu/DejaVuSans-Bold.ttf" "$OUT/"
cp "$R/docs/icon/icon_1024.png" "$OUT/icon.png"
cp "$S/tools/za/za-default.ini" "$OUT/za-default.ini"
cp "$S/README.md" "$S/LUMIOSE.md" "$S/LUMIOSE-RELEASE.md" "$OUT/docs/" && cp "$S/docs/PC-CONTROLS.md" "$OUT/docs/" 2>/dev/null || true
cp "$S/LICENSE" "$OUT/licenses/ZARecomp-Online-LICENSE.txt"
cp "$F/Baloo2/OFL.txt" "$OUT/licenses/Baloo2-OFL.txt"
cp "$F/BIZUDPGothic/OFL.txt" "$OUT/licenses/BIZUDPGothic-OFL.txt"
cp "$F/DejaVu/LICENSE" "$OUT/licenses/DejaVu-LICENSE.txt"
for d in "$S"/third_party/*/; do  # (the components its runtime is built with keep their licenses)
    n=$(basename "$d")
    for f in "$d"LICENSE* "$d"COPYING* "$d"license*; do [ -f "$f" ] && cp "$f" "$OUT/licenses/$n-$(basename "$f")"; done
done
rm -f "$OUT"/licenses/ooz-*  # (not shipped: zaprompts isn't)
install -m755 "$R/tools/linux/online/start.sh" "$OUT/start.sh"
cp "$R/tools/linux/online/run_entry.py" "$OUT/run_entry.py"
echo "$REV" > "$OUT/VERSION"
if find "$OUT" -iname 'lasgame*' -o -iname 'romfs.bin' -o -iname '*.nsp' -o -iname '*.xci' -o -iname '*.nca' | grep .; then
    echo "build_online: game files in $OUT (above)"; exit 1
fi
echo "build_online: $OUT ($(du -sh "$OUT" | cut -f1))"
