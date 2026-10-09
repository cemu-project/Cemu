#!/usr/bin/env bash
# The root half of scripts/install.sh (run that one): copies what the GamePad network needs out of the repo into
# /usr/local/libexec/cemu-gamepad (system services can't run files from a home directory under SELinux), and installs
# cemu-gamepad-net.service plus the udev rule that starts it whenever the RT5572 is plugged in.
set -euo pipefail
[ "$(id -u)" -eq 0 ] || { echo "run scripts/install.sh (as yourself) instead" >&2; exit 1; }
root=$(cd "$(dirname "$0")/.." && pwd)
dest=/usr/local/libexec/cemu-gamepad
new=$dest.new
rm -rf "$new"
mkdir -p "$new/scripts" "$new/harness" "$new/build/drc-hostap/hostapd"
install -m 755 "$root/scripts/pad-net.sh" "$new/scripts/"
install -m 644 "$root/harness/dnsmasq.conf" "$new/harness/"
install -m 600 "$root/harness/hostapd-normal.conf" "$new/harness/"   # template; the key comes from /etc/cemu-gamepad/pad.conf
install -m 755 "$root/build/drc-hostap/hostapd/hostapd" "$new/build/drc-hostap/hostapd/"
rm -rf "$dest"; mv "$new" "$dest"
restorecon -R "$dest" 2>/dev/null || true

install -m 644 "$root/packaging/cemu-gamepad-net.service" /etc/systemd/system/
# GamePad setup writes a rule for the adapter the user chose; the packaged one (any rt2800usb) is the default.
[ -f /etc/cemu-gamepad/setup ] || install -m 644 "$root/packaging/70-cemu-gamepad.rules" /etc/udev/rules.d/
systemctl daemon-reload
udevadm control --reload
# A pad-net.sh still running by hand holds the adapter; the service takes over from it.
rt=$(for d in /sys/class/net/*; do [ "$(basename "$(readlink -f "$d/device/driver" 2>/dev/null)")" = rt2800usb ] && basename "$d"; done | head -1)
if [ "${1:-}" = --no-start ]; then
  :   # GamePad setup starts it itself, once the adapter is free
elif pgrep -f "^$root/build/drc-hostap/hostapd/hostapd " >/dev/null; then
  echo "   a pad-net.sh is running by hand: stop it (Ctrl-C in its terminal), then: sudo systemctl start cemu-gamepad-net"
elif [ -n "$rt" ]; then
  # start it now, as udev does on plug-in
  systemctl restart cemu-gamepad-net.service
  echo "   cemu-gamepad-net.service started on $rt"
else
  echo "   the RT5572 isn't plugged in; the network starts when it is"
fi
echo "   system part installed: $dest, cemu-gamepad-net.service, /etc/udev/rules.d/70-cemu-gamepad.rules"
