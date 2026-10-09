#!/usr/bin/env bash
# Build drc-x264 and libdrc from third_party/ into build/, applying patches/libdrc/*.patch.
# third_party/ stays untouched (read-only references). Compile only; nothing is run.
# Needs: yasm make g++ pkg-config patch rsync, and libswscale/libavutil development files.
#
# x264 is built twice. build/prefix: --disable-asm, PIC, so libdrc.so links (the 2013 assembly isn't
# position-independent). build/prefix-asm: with assembly, for the bridge, which links it statically
# into -no-pie executables. Measured 5.5x faster (docs/LIBDRC-BUILD.md, docs/MEASUREMENTS.md).

set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
build=$root/build
prefix=$build/prefix
jobs=${JOBS:-4}

for tool in yasm make g++ pkg-config patch rsync; do
  command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 1; }
done
pkg-config --exists libswscale || { echo "missing libswscale (dnf install ffmpeg-free-devel)" >&2; exit 1; }

rm -rf "$build/drc-x264" "$build/libdrc" "$prefix"
mkdir -p "$build/drc-x264" "$prefix"

# drc-x264 source + patches/drc-x264/*.patch, built out of tree from this copy (third_party stays untouched)
x264src=$build/drc-x264-src
rm -rf "$x264src"
rsync -a --exclude .git "$root/third_party/drc-x264/" "$x264src/"
for p in "$root"/patches/drc-x264/*.patch; do
  [ -e "$p" ] || continue
  echo "   applying $(basename "$p")"
  patch -d "$x264src" -p1 --quiet --forward < "$p"
done

echo "== drc-x264"
(
  cd "$build/drc-x264"
  "$x264src/configure" --prefix="$prefix" \
    --enable-static --enable-pic --disable-cli --disable-asm > configure.log
  make -j"$jobs" > make.log 2>&1
  make install-lib-static > install.log 2>&1
)

echo "== drc-x264 with assembly (non-PIC; for the bridge, linked -no-pie)"
rm -rf "$build/drc-x264-asm" "$build/prefix-asm"
mkdir -p "$build/drc-x264-asm"
(
  cd "$build/drc-x264-asm"
  "$x264src/configure" --prefix="$build/prefix-asm" \
    --enable-static --disable-cli > configure.log
  make -j"$jobs" > make.log 2>&1
  make install-lib-static > install.log 2>&1
)

echo "== libdrc"
rsync -a --exclude .git "$root/third_party/libdrc/" "$build/libdrc/"
(
  cd "$build/libdrc"
  for p in "$root"/patches/libdrc/*.patch; do
    echo "   applying $(basename "$p")"
    patch -p1 --quiet --forward < "$p"
  done
  PKG_CONFIG_PATH="$prefix/lib/pkgconfig" ./configure --cxx=g++ --disable-demos > configure.log
  make -j"$jobs" libdrc > make.log 2>&1
)

# A shared object links fine with missing symbols; check explicitly so it fails here, not at load time.
if ldd -r "$build/libdrc/libdrc.so" 2>&1 | grep -q 'undefined symbol'; then
  echo "libdrc.so has undefined symbols:" >&2
  ldd -r "$build/libdrc/libdrc.so" 2>&1 | grep 'undefined symbol' >&2
  exit 1
fi

echo "== ok: $build/libdrc/libdrc.so, $build/libdrc/libdrc.a, $prefix-asm/lib/libx264.a"
