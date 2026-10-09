#!/usr/bin/env bash
# Builds drc-hostap (hostapd + wpa_supplicant, CONFIG_TENDONIN) into build/drc-hostap with patches/hostapd/*.patch
# applied. The source in third_party/drc-hostap-vanilla stays untouched. Building in a container? Prefix the build
# commands: RUN="distrobox enter my-box --" ./scripts/build-drc-hostap.sh
# Full rebuild every time (rsync restores pristine sources, so stale objects would not be noticed by make).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
src=$root/third_party/drc-hostap-vanilla
out=$root/build/drc-hostap
rsync -a --delete --exclude .git "$src/" "$out/"
for p in "$root"/patches/hostapd/*.patch; do
  echo "applying $(basename "$p")"
  ${RUN:-} patch -d "$out" -p1 --forward --no-backup-if-mismatch -i "$p"
done
cp "$out/conf/hostapd.config" "$out/hostapd/.config"
cp "$out/conf/wpa_supplicant.config" "$out/wpa_supplicant/.config"
${RUN:-} make -C "$out/hostapd" -j4
${RUN:-} make -C "$out/wpa_supplicant" -j4
