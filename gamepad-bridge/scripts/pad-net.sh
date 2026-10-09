#!/usr/bin/env bash
# The GamePad's network, for playing: the RT5572 (rt2800usb) as the normal-mode AP the pad is paired to, 192.168.1.10,
# DHCP (pad = 192.168.1.11), firewalld zone trusted for that interface. Root-only parts only: the bridge and Cemu
# run as you (scripts/play.sh). Stays up until Ctrl-C,
# then hands everything back. The AX200 keeps the internet. Logs: runs/<timestamp>-padnet/
#
#   sudo ./scripts/pad-net.sh            # terminal 1, leave it running
#   ./scripts/play.sh                    # terminal 2
#
# Installed (scripts/install.sh), it runs as cemu-gamepad-net.service whenever the RT5572 is plugged in (SERVICE=1:
# logs in /var/log/cemu-gamepad, no packet captures unless CAPTURE=1, stops cleanly when the adapter is unplugged).
# Either way it keeps /run/cemu-gamepad/pad up to date for the bridge: connected / disconnected (the GamePad's
# Wi-Fi link, from hostapd's events) / down (this network stopped).
set -uo pipefail
[ "$(id -u)" -eq 0 ] || { echo "run as root: sudo $0" >&2; exit 1; }
# This PC's console identity, written by GamePad setup (scripts/gamepad-setup.sh): AP_MAC (the address the pad was
# paired to), PSK (the Wi-Fi key it was given), ADAPTER_MAC (the chosen adapter's factory address).
PSK="" ADAPTER_MAC=""
[ -r /etc/cemu-gamepad/pad.conf ] && . /etc/cemu-gamepad/pad.conf
[ -n "${AP_MAC:-}" ] && [ -n "$PSK" ] || { echo "the GamePad isn't set up: Cemu > Options > General settings > GamePad" >&2; exit 0; }
# Channel 165 (5825 MHz), not 36: home routers often use 36-64 at 80/160 MHz, and sharing the air with the PC's own
# internet traffic cost 40-70 video resyncs per second in testing. 165 is outside the 149-161 block too, and it's the
# channel a real Wii U used.
CHAN=${CHAN:-165}
root=$(cd "$(dirname "$0")/.." && pwd)
hostapd=$root/build/drc-hostap/hostapd/hostapd
SERVICE=${SERVICE:-0}
CAPTURE=${CAPTURE:-$([ "$SERVICE" = 1 ] && echo 0 || echo 1)}
if [ "$SERVICE" = 1 ]; then
  logs=/var/log/cemu-gamepad
  mkdir -p "$logs"
  ls -1d "$logs"/*-padnet 2>/dev/null | head -n -9 | xargs -r rm -rf   # keep the last 10 sessions
  R=$logs/$(date +%Y%m%d-%H%M%S)-padnet
else
  R=$root/runs/$(date +%Y%m%d-%H%M%S)-padnet
fi
STATUS_DIR=/run/cemu-gamepad
pad_status() { mkdir -p "$STATUS_DIR"; echo "$1" > "$STATUS_DIR/pad.tmp" && mv -f "$STATUS_DIR/pad.tmp" "$STATUS_DIR/pad"; }
run=$R/run
mkdir -p "$run"
exec > >(tee -a "$R/padnet.log") 2>&1
rtif() {
  local d p
  if [ -n "$ADAPTER_MAC" ]; then   # the adapter setup chose, by factory address ("permaddr" while we've changed it)
    for d in /sys/class/net/*; do
      [ -e "$d/phy80211" ] && [ "$(cat "$d/type")" = 1 ] || continue
      p=$(ip link show dev "$(basename "$d")" | sed -n 's/.*permaddr \([0-9a-f:]*\).*/\1/p'); p=${p:-$(cat "$d/address")}
      [ "$p" = "$ADAPTER_MAC" ] && { basename "$d"; return; }
    done
    return
  fi
  for d in /sys/class/net/*; do [ "$(basename "$(readlink -f "$d/device/driver" 2>/dev/null)")" = rt2800usb ] && [ "$(cat "$d/type")" = 1 ] && basename "$d"; done | head -1
}
# A monitor interface left on the RT5572 (console-resync-capture.sh's mon0) isn't the AP interface, and keeps the
# radio from becoming an AP: remove any.
for d in /sys/class/net/*; do
  [ "$(basename "$(readlink -f "$d/device/driver" 2>/dev/null)")" = rt2800usb ] && [ "$(cat "$d/type")" != 1 ] && iw dev "$(basename "$d")" del && echo "removed leftover monitor interface $(basename "$d")"
done

if pgrep -f "drc-hostap/hostapd/hostapd " >/dev/null; then   # by hand or the installed service
  echo "a pad network (hostapd) is already running, probably an earlier pad-net.sh:" >&2
  pgrep -fa "drc-hostap/hostapd/hostapd " | sed 's/^/   /' >&2
  systemctl is-active -q cemu-gamepad-net.service && echo "it's the installed service: sudo systemctl stop cemu-gamepad-net" >&2
  echo "Ctrl-C it in its terminal, or if that terminal is gone:  sudo pkill -f build/drc-hostap/hostapd/hostapd; sudo pkill -f 'dnsmasq --conf-file=$root/runs'" >&2
  exit 1
fi
IF=$(rtif)
[ -n "$IF" ] || { echo "the GamePad's Wi-Fi adapter isn't plugged in" >&2; exit 0; }
# The pad needs packets stamped with this adapter's TSF. Stock kernels don't export it; libdrc (patch 0008) then
# reads it over USB, which needs read-write access to the adapter's /dev/bus/usb node: a runtime udev rule tags this
# adapter (its vendor:product) uaccess, so the logged-in user's bridge gets it. (The patched mac80211's
# /sys/class/net/<if>/tsf is still used if it's loaded.)
usbdev=$(readlink -f "/sys/class/net/$IF/device/..")
if [ ! -e "/sys/class/net/$IF/tsf" ]; then
  vid=$(cat "$usbdev/idVendor") pid=$(cat "$usbdev/idProduct")
  mkdir -p /run/udev/rules.d
  echo "SUBSYSTEM==\"usb\", ENV{DEVTYPE}==\"usb_device\", ATTR{idVendor}==\"$vid\", ATTR{idProduct}==\"$pid\", TAG+=\"uaccess\"" \
    > /run/udev/rules.d/71-cemu-gamepad-tsf.rules
  udevadm control --reload
  udevadm trigger --action=change --settle "$usbdev"
  echo "== TSF read over USB ($vid:$pid, /dev/bus/usb/$(printf %03d/%03d "$(cat "$usbdev/busnum")" "$(cat "$usbdev/devnum")"))"
fi

pids=(); fw=0
restore() {
  echo; echo "== stopping the pad network"
  pad_status down
  for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done
  [ -f "$run/dnsmasq.pid" ] && kill "$(cat "$run/dnsmasq.pid")" 2>/dev/null
  sleep 1
  [ "$fw" = 1 ] && firewall-cmd -q --zone=trusted --remove-interface="$IF"
  ip addr flush dev "$IF" 2>/dev/null; ip link set mtu 1500 dev "$IF" 2>/dev/null
  perm=$(ethtool -P "$IF" 2>/dev/null | awk '{print $3}')
  [ -n "$perm" ] && { ip link set "$IF" down; ip link set "$IF" address "$perm"; ip link set "$IF" up; }
  nmcli device set "$IF" managed yes >/dev/null && echo "   $IF handed back to NetworkManager"
  [ -n "${SUDO_USER:-}" ] && chown -R "$SUDO_USER": "$R"
  echo "   logs: $R"
}
trap restore EXIT
trap 'exit 0' INT TERM HUP   # HUP: closing the terminal also cleans up

pad_status disconnected
nmcli device set "$IF" managed no || exit 1
sleep 1
ip link set "$IF" down; ip addr flush dev "$IF"; ip link set "$IF" address "$AP_MAC"; ip link set "$IF" up
# firewalld's default zone drops DHCP (udp/67) from the pad (runs/20261008-191614-normal)
if systemctl is-active -q firewalld; then firewall-cmd -q --zone=trusted --change-interface="$IF" && fw=1; fi
mac=$(echo "$AP_MAC" | tr -d :)
for f in hostapd-normal.conf dnsmasq.conf; do
  sed -e "s|@IFACE@|$IF|g" -e "s|@RUNDIR@|$run|g" -e "s|@MAC11@|${mac:0:11}|g" -e "s|@MAC@|$mac|g" "$root/harness/$f" > "$run/$f"
done
sed -i "s|^channel=.*|channel=$CHAN|" "$run/hostapd-normal.conf"
[ -n "$PSK" ] && sed -i "s|^wpa_psk=.*|wpa_psk=$PSK|" "$run/hostapd-normal.conf"
# Warn when the laptop's other Wi-Fi overlaps the pad's channel (same air, same antennas a few cm apart).
pad_mhz=$(( CHAN <= 14 ? 2407 + 5 * CHAN : 5000 + 5 * CHAN ))
for other in $(iw dev | awk '/Interface/{print $2}'); do
  [ "$other" = "$IF" ] && continue
  line=$(iw dev "$other" info 2>/dev/null | grep -E 'channel [0-9]+')
  [ -n "$line" ] || continue
  center=$(echo "$line" | sed -n 's/.*center1: \([0-9]*\) MHz.*/\1/p'); width=$(echo "$line" | sed -n 's/.*width: \([0-9]*\) MHz.*/\1/p')
  [ -n "$center" ] && [ -n "$width" ] || continue
  if [ $(( pad_mhz + 10 )) -gt $(( center - width / 2 )) ] && [ $(( pad_mhz - 10 )) -lt $(( center + width / 2 )) ]; then
    echo "   WARNING: $other is on $line"
    echo "            which overlaps the pad's channel $CHAN. Expect lost video. Pick another CHAN=, or move $other."
  fi
done
ip addr add 192.168.1.10/24 dev "$IF"
ip link set mtu 1800 dev "$IF" || echo "   note: MTU 1800 refused (libdrc network.rst:131 asks for it)"
"$hostapd" -dd -t "$run/hostapd-normal.conf" > "$R/hostapd.log" 2>&1 & pids+=($!)
for _ in $(seq 1 30); do grep -qE 'AP-ENABLED|AP-DISABLED|Unable to setup' "$R/hostapd.log" && break; sleep 0.5; done
grep -q AP-ENABLED "$R/hostapd.log" || { echo "the AP did not come up:"; grep -E 'Unable|not allowed|failed' "$R/hostapd.log" | head -5; exit 1; }
dnsmasq --conf-file="$run/dnsmasq.conf" --pid-file="$run/dnsmasq.pid" --log-facility="$R/dnsmasq.log" || exit 1

echo
echo "== GamePad network up: $IF as $AP_MAC, ch $CHAN, 192.168.1.10"
if [ "$SERVICE" != 1 ]; then
  echo "   Now, in another terminal (not root):  ./scripts/play.sh"
  echo "   Then turn the GamePad on. Ctrl-C here when you're done playing."
fi
echo
tail -n0 -f "$R/hostapd.log" | grep --line-buffered -E 'AP-STA-CONNECTED|AP-STA-DISCONNECTED' | while read -r l; do
  echo "$l"
  case $l in *AP-STA-CONNECTED*) pad_status connected;; *) pad_status disconnected;; esac
done &
pids+=($!)
if [ "$CAPTURE" = 1 ]; then
# What we send the pad (video + video-format packets) and its resync requests, to compare with the console's stream
# (scripts/vstrm-stats.py). Headers only for video (-s 96) to keep the file small.
# FULL=1 records whole video packets (to decode our own stream afterwards, e.g. to look for tearing)
tcpdump -i "$IF" -U -s "$([ "${FULL:-0}" = 1 ] && echo 0 || echo 96)" -w "$R/pad-video.pcap" "udp dst port 50120 or udp dst port 50121 or udp dst port 50010" \
  2> "$R/tcpdump.log" & pids+=($!)
# Our AP's TSF next to laptop time, for scripts/console-timing.py (when do our chunks arrive vs their timestamp)
( while sleep 0.02; do printf '%s %s\n' "$(date +%s.%N)" "$(od -An -t u8 -N8 "/sys/class/net/$IF/tsf" 2>/dev/null | tr -d ' ')"; done ) > "$R/tsf.txt" & pids+=($!)
# The link to the pad, every 5 s: TX bitrate/retries/failures (a pixelated or resyncing picture can be airtime)
( while sleep 5; do { date +%T; iw dev "$IF" station dump; echo; } >> "$R/stations.txt"; done ) & pids+=($!)
fi
# Until hostapd exits (an error: exit 1, the service restarts it) or the adapter is unplugged (exit 0).
while kill -0 "${pids[0]}" 2>/dev/null; do
  [ -e "/sys/class/net/$IF" ] || { echo "the RT5572 was unplugged"; exit 0; }
  sleep 2
done
echo "hostapd exited"
exit 1
