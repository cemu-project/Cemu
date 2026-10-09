#!/usr/bin/env bash
# One-step GamePad play: after this, plug in the RT5572, turn the GamePad on, start a Wii U game from ES-DE (or Cemu).
#   - system (asks for sudo): cemu-gamepad-net.service, started whenever the RT5572 is plugged in
#     (scripts/install-system.sh)
#   - you: cemu-gamepad-bridge.service (systemd --user, from login on), and ~/Applications/Cemu/Cemu -> this repo's
#     Cemu build, which is where ES-DE looks for Cemu ("Cemu (Standalone)": Cemu -g <rom>)
# Re-run after rebuilding hostapd (the system part runs copies). The bridge and Cemu run
# straight from this repo: rebuild them, then  systemctl --user restart cemu-gamepad-bridge.
#
#   ./scripts/install.sh            ./scripts/install.sh user   (no sudo part)            ./scripts/install.sh uninstall
set -euo pipefail
[ "$(id -u)" -ne 0 ] || { echo "run as yourself (it asks for sudo for the system part)" >&2; exit 1; }
root=$(cd "$(dirname "$0")/.." && pwd)
bridge=$root/build/bridge/cemu-gamepad-bridge
cemu=$(cd "$root/.." && pwd)/bin/Cemu_release   # this fork's Cemu build
unit=$HOME/.config/systemd/user/cemu-gamepad-bridge.service

if [ "${1:-}" = uninstall ]; then
  systemctl --user disable --now cemu-gamepad-bridge.service 2>/dev/null || true
  rm -f "$unit"; systemctl --user daemon-reload
  [ -L "$HOME/Applications/Cemu/Cemu" ] && rm "$HOME/Applications/Cemu/Cemu" && rmdir "$HOME/Applications/Cemu" 2>/dev/null
  [ -L "$HOME/.local/bin/cemu-gamepad-setup" ] && rm "$HOME/.local/bin/cemu-gamepad-setup"
  sudo sh -c 'systemctl stop cemu-gamepad-net.service; rm -f /etc/systemd/system/cemu-gamepad-net.service /etc/udev/rules.d/70-cemu-gamepad.rules; rm -rf /usr/local/libexec/cemu-gamepad /etc/cemu-gamepad; systemctl daemon-reload; udevadm control --reload'
  echo "uninstalled"; exit 0
fi

for f in "$bridge" "$cemu" "$root/build/drc-hostap/hostapd/hostapd"; do
  [ -x "$f" ] || { echo "missing $f: build it first" >&2; exit 1; }
done
# "user": only your part (Cemu's Set up GamePad window runs this after its own root part).
if [ "${1:-}" != user ]; then
  echo "== system part (sudo)"
  sudo "$root/scripts/install-system.sh"
fi

echo "== bridge service (you)"
if pgrep -f "^$bridge( |$)" >/dev/null && ! systemctl --user is-active -q cemu-gamepad-bridge.service; then
  echo "   a bridge is running by hand (play.sh?); close Cemu / stop it, then run this again" >&2; exit 1
fi
mkdir -p "$(dirname "$unit")"
sed "s|@BRIDGE@|$bridge|" "$root/packaging/cemu-gamepad-bridge.service" > "$unit"
systemctl --user daemon-reload
systemctl --user enable cemu-gamepad-bridge.service >/dev/null
systemctl --user restart --no-block cemu-gamepad-bridge.service   # it waits for the GamePad network; don't wait with it
echo "   cemu-gamepad-bridge.service enabled and started"

echo "== Cemu for ES-DE"
# Cemu's "Set up GamePad" window runs this helper (GamePadSetupDialog.cpp looks here first).
mkdir -p "$HOME/.local/bin"
ln -sfn "$root/scripts/gamepad-setup.sh" "$HOME/.local/bin/cemu-gamepad-setup"
echo "   $HOME/.local/bin/cemu-gamepad-setup -> $root/scripts/gamepad-setup.sh"
mkdir -p "$HOME/Applications/Cemu"
if [ -e "$HOME/Applications/Cemu/Cemu" ] && [ ! -L "$HOME/Applications/Cemu/Cemu" ]; then
  echo "   $HOME/Applications/Cemu/Cemu exists and isn't ours; left alone" >&2
else
  ln -sfn "$cemu" "$HOME/Applications/Cemu/Cemu"
  echo "   $HOME/Applications/Cemu/Cemu -> $cemu"
fi
echo
echo "Done. Plug in the RT5572 (if it isn't), turn the GamePad on, start a Wii U game from ES-DE."
echo "Status:  systemctl status cemu-gamepad-net;  systemctl --user status cemu-gamepad-bridge"
