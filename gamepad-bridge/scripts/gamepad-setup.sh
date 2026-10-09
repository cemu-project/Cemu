#!/usr/bin/env bash
# GamePad setup helper, driven by Cemu's "Set up GamePad" window (and usable by hand).
#
#   gamepad-setup.sh list               no root. One line per Wi-Fi adapter, tab-separated:
#                                       ADAPTER <iface> <name> <verdict> <reason> <internet 0|1>
#                                       verdict: works | maybe | no; reason is short and user-facing.
#   gamepad-setup.sh status             no root. "SETUP none" or "SETUP done <adapter name>", then "PAD <state>"
#                                       (connected | disconnected | down | off).
#   pkexec gamepad-setup.sh setup IF    root. Checks IF, pairs the GamePad, installs the GamePad network service.
#                                       stdout, for the window: "STAGE check", "STAGE sync <4 symbols>",
#                                       "STAGE finish", last line "RESULT works" or "RESULT fails <short reason>".
#                                       Closing stdin cancels. Details: /var/log/cemu-gamepad/<ts>-setup.log
#   gamepad-setup.sh install-user       no root. The bridge's user service (after a successful setup).
#   sudo gamepad-setup.sh reset         root. Undo setup (network off, pairing forgotten, adapter given back).
#
# What an adapter needs (docs/STATUS.md, docs/PROTOCOL.md "TSF"):
#  1. host a 5 GHz access point (the pad only uses 5 GHz), on a channel the regulatory domain allows us to start on;
#  2. give us its TSF clock, which every video/audio packet is stamped with. Measured 2026-10-09: a substitute clock
#     (CLOCK_MONOTONIC) makes the pad reject every frame and leave the Wi-Fi. Readable today: the patched mac80211's
#     /sys/class/net/<if>/tsf, or rt2800usb over USB (libdrc patch 0008). Drivers that stamp received frames with the
#     TSF (ath9k, newer mt76) could work later; not implemented, so they're "no" for now.
set -uo pipefail
root=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
hostapd=$root/build/drc-hostap/hostapd/hostapd
hostapd_cli=$root/build/drc-hostap/hostapd/hostapd_cli
tsf_probe=$root/build/bridge/tsf-probe
etc=/etc/cemu-gamepad
PAD_CHANNELS="36 40 44 48 149 153 157 161 165"   # 5 GHz channels without radar detection; 165 is what we play on
PAIR_CHANNELS="36 44 149"   # the pad found the pairing AP on 36 in ~6 s every time (docs/PAIRING.md:11, :82)

# 5 GHz pad channels this phy can start an AP on: listed, not disabled, no "no IR" (may not initiate radiation),
# no radar detection.
ap_channels() {
  local info; info=$(iw phy "$1" channels 2>/dev/null) || return
  # One record per "* <MHz> MHz [<ch>]" line plus its indented flag lines; keep pad channels with no blocking flag.
  printf '%s\n' "$info" | awk -v want=" $PAD_CHANNELS " '
    function flush() { if (ch != "" && !bad && index(want, " " ch " ")) out = out (out == "" ? "" : " ") ch }
    /^[[:space:]]*\* [0-9.]+ MHz \[[0-9]+\]/ {
      flush(); ch = $0; sub(/.*\[/, "", ch); sub(/\].*/, "", ch); bad = ($0 ~ /disabled/); next }
    tolower($0) ~ /no ir|no-ir|radar/ { bad = 1 }
    END { flush(); print out }'
}

supports_ap() { iw phy "$1" info 2>/dev/null | sed -n '/Supported interface modes/,/^\s*[A-Z]/p' | grep -q -E '^\s*\* AP$'; }
has_5ghz() { iw phy "$1" info 2>/dev/null | grep -q -E '^\s*\* 5[0-9]{3}(\.0)? MHz'; }

# How we can read this adapter's TSF: sysfs | usb | rx (not implemented) | none
clock_method() {
  local ifc=$1 drv=$2
  [ -e "/sys/class/net/$ifc/tsf" ] && { echo sysfs; return; }
  case $drv in
    rt2800usb) echo usb ;;
    ath9k|ath9k_htc|mt7603e|mt7615e|mt7663u|mt7915e|mt7921e|mt7921u|mt7921s|mt7925e|mt7925u|mt7996e) echo rx ;;
    *) echo none ;;
  esac
}

adapter_name() {
  local dev=$1 bus=$2
  if [ "$bus" = usb ]; then
    local u; u=$(readlink -f "$dev/..")
    echo "$(cat "$u/manufacturer" 2>/dev/null) $(cat "$u/product" 2>/dev/null)" | sed 's/^ *//; s/ *$//; s/^$/USB Wi-Fi adapter/'
  elif [ "$bus" = pci ] && command -v lspci >/dev/null; then
    lspci -s "$(basename "$(readlink -f "$dev")")" 2>/dev/null | sed 's/^[^:]*:[^:]*: //; s/ Corporation//; s/ (rev [0-9a-f]*)//'
  else
    echo "Wi-Fi adapter"
  fi
}

# The adapter's factory address, even while we've given it the console's (ip shows "permaddr" then).
perm_mac() {
  local p; p=$(ip link show dev "$1" 2>/dev/null | sed -n 's/.*permaddr \([0-9a-f:]*\).*/\1/p')
  [ -n "$p" ] && echo "$p" || cat "/sys/class/net/$1/address"
}

# verdict + short reason for one interface; sets V and WHY
judge() {
  local ifc=$1 drv phy method
  drv=$(basename "$(readlink -f "/sys/class/net/$ifc/device/driver" 2>/dev/null)")
  phy=$(basename "$(readlink -f "/sys/class/net/$ifc/phy80211")")
  method=$(clock_method "$ifc" "$drv")
  if ! supports_ap "$phy" || ! has_5ghz "$phy"; then V=no WHY="Can't host the GamePad's network"
  elif [ "$method" = none ]; then V=no WHY="Not compatible"
  elif [ "$method" = rx ]; then V=no WHY="Not compatible yet"
  elif [ -z "$(ap_channels "$phy")" ]; then V=maybe WHY="Might work"
  else V=works WHY="Ready"
  fi
}

cmd_list() {
  local d ifc bus inet net
  inet=$(ip route show default 2>/dev/null | awk '{for (i = 1; i < NF; i++) if ($i == "dev") print $(i + 1)}' | sort -u)
  for d in /sys/class/net/*; do
    ifc=$(basename "$d")
    [ -e "$d/phy80211" ] && [ "$(cat "$d/type")" = 1 ] || continue   # Wi-Fi, not a monitor interface
    bus=$(basename "$(readlink -f "$d/device/subsystem" 2>/dev/null)")
    judge "$ifc"
    net=0; case " $inet " in *" $ifc "*) net=1;; esac
    printf 'ADAPTER\t%s\t%s\t%s\t%s\t%s\n' "$ifc" "$(adapter_name "$d/device" "$bus")" "$V" "$WHY" "$net"
  done
}

cmd_status() {
  if [ -r "$etc/setup" ]; then
    printf 'SETUP done\t%s\n' "$(sed -n 's/^NAME=//p' "$etc/setup")"
  else
    echo "SETUP none"
  fi
  echo "PAD $(cat /run/cemu-gamepad/pad 2>/dev/null || echo off)"
}

cmd_install_user() {
  "$root/scripts/install.sh" user
}

# ---- setup (root) ----
gui() { echo "$*" >&3; }
fail() { gui "RESULT fails $1"; echo "FAILED: $1"; exit 1; }
# Cancelled when the window closes our stdin (EOF; read's status 1). Waits up to $1 s, or until the pattern $2
# shows up in file $3. Returns 1 if cancelled.
wait_or_cancel() {
  local deadline=$((SECONDS + $1)) line rv
  while [ $SECONDS -lt $deadline ]; do
    read -r -t 0.5 line; rv=$?
    [ $rv -eq 1 ] && return 1
    [ -n "${2:-}" ] && grep -qE "$2" "$3" 2>/dev/null && return 0
  done
  return 0
}

cmd_setup() {
  local ifc=$1
  exec 3>&1
  [ "$(id -u)" -eq 0 ] || { gui "RESULT fails needs administrator rights"; exit 1; }
  mkdir -p /var/log/cemu-gamepad
  local log; log=/var/log/cemu-gamepad/$(date +%Y%m%d-%H%M%S)-setup.log
  install -m 644 /dev/null "$log"   # readable: the Wi-Fi key is never traced into it (set +x around it)
  exec >> "$log" 2>&1
  set -x
  gui "STAGE check"
  [ -e "/sys/class/net/$ifc/phy80211" ] || fail "the adapter is gone (unplugged?)"
  [ -x "$hostapd" ] && [ -x "$hostapd_cli" ] && [ -x "$tsf_probe" ] || fail "the GamePad bridge isn't installed correctly"
  judge "$ifc"
  [ "$V" = no ] && fail "$WHY"
  # Not local: restore() runs from the EXIT trap after this function has returned, when its locals are gone
  # (set -u then killed restore halfway on the first successful run: no network restart, Cemu waited forever).
  local phy chans name
  ifc_g=$ifc perm="" tmp="" svc_was_active=0 hp="" done_ok=0
  phy=$(basename "$(readlink -f "/sys/class/net/$ifc/phy80211")")
  name=$(adapter_name "/sys/class/net/$ifc/device" "$(basename "$(readlink -f "/sys/class/net/$ifc/device/subsystem")")")
  perm=$(perm_mac "$ifc")
  tmp=$(mktemp -d /run/cemu-gamepad-setup.XXXXXX)
  stop_ap() { [ -n "${hp:-}" ] && kill "$hp" 2>/dev/null && wait "$hp" 2>/dev/null; hp=""; }
  restore() {
    stop_ap
    ip addr del 192.168.1.10/24 dev "$ifc_g" 2>/dev/null
    # Back to a plain station before anyone else gets it: NetworkManager's wpa_supplicant crashed (SEGV, every 10 s,
    # taking the laptop's internet down each time, until a reboot) when it was handed this adapter still in AP mode.
    ip link set "$ifc_g" down; iw dev "$ifc_g" set type managed; ip link set "$ifc_g" address "$perm"; ip link set "$ifc_g" up
    rm -rf "$tmp"
    if [ "$done_ok" = 1 ]; then
      systemctl restart --no-block cemu-gamepad-net.service   # takes over the adapter with the new pairing
    else
      nmcli device set "$ifc_g" managed yes
      [ "$svc_was_active" = 1 ] && systemctl start --no-block cemu-gamepad-net.service
    fi
  }
  trap restore EXIT
  if systemctl is-active -q cemu-gamepad-net.service; then
    svc_was_active=1
    systemctl stop cemu-gamepad-net.service
  fi
  pgrep -f "hostapd.*$ifc" && fail "another program is using this adapter"
  # Always take it from NetworkManager, after the service stop above: stopping the service hands it back to NM, and
  # an adapter NM still manages can't start an access point ("can't host" on the first real run, 2026-10-09).
  nmcli device set "$ifc" managed no; sleep 1

  # 1. Can it host the network, and can we read its clock? (the same libdrc code the bridge uses)
  chans=$(ap_channels "$phy")
  local ch=${chans##* }; ch=${ch:-165}
  printf 'interface=%s\ndriver=nl80211\nssid=CemuGamePadTest\nhw_mode=a\nchannel=%s\nieee80211n=1\nwmm_enabled=1\n' \
    "$ifc" "$ch" > "$tmp/test.conf"
  "$hostapd" -dd -t "$tmp/test.conf" > "$tmp/test.log" 2>&1 & hp=$!   # test network: no key, so the full log is fine
  for _ in $(seq 1 30); do   # hostapd's verdict, or it exited (-dd prints harmless "Could not ..." lines too)
    grep -qE 'AP-ENABLED|AP-DISABLED' "$tmp/test.log" && break; kill -0 "$hp" 2>/dev/null || break; sleep 0.5
  done
  cat "$tmp/test.log"
  grep -q AP-ENABLED "$tmp/test.log" || fail "this adapter can't host the GamePad's network"
  ip addr add 192.168.1.10/24 dev "$ifc"   # libdrc finds the adapter by this address, as the bridge does
  local out; out=$("$tsf_probe" 2>&1); echo "$out"
  local n=0 good=0 x
  for x in $(printf '%s\n' "$out" | grep -o '+[0-9]* us' | tr -dc '0-9\n'); do
    n=$((n + 1)); [ "$x" -gt 190000 ] && [ "$x" -lt 260000 ] && good=$((good + 1))
  done
  [ $n -gt 0 ] && [ $good -eq $n ] || fail "this adapter isn't compatible"
  ip addr del 192.168.1.10/24 dev "$ifc"
  stop_ap

  # 2. This PC's own console identity: a Nintendo-OUI address (the pad expects a console's; docs/PAIRING.md) and a
  #    random Wi-Fi key. The symbols on the pad come from the address's last byte (as padday-rt5572.sh pin_for).
  { set +x; } 2>/dev/null   # the key never goes in the log (it's readable, for troubleshooting)
  local ap_mac psk pin syms
  ap_mac=34:af:2c:$(od -An -tx1 -N3 /dev/urandom | awk '{print $1":"$2":"$3}')
  psk=$(od -An -tx1 -N32 /dev/urandom | tr -d ' \n')
  local b=$((16#${ap_mac##*:})); local sym=(SPADE HEART DIAMOND CLUB)
  pin="$((b >> 6))$(((b >> 4) & 3))$(((b >> 2) & 3))$((b & 3))5678"
  syms="${sym[${pin:0:1}]} ${sym[${pin:1:1}]} ${sym[${pin:2:1}]} ${sym[${pin:3:1}]}"
  local mac; mac=$(echo "$ap_mac" | tr -d :)
  for f in hostapd-normal.conf hostapd-pairing.conf; do
    sed -e "s|@IFACE@|$ifc|g" -e "s|@RUNDIR@|$tmp|g" -e "s|@MAC11@|${mac:0:11}|g" -e "s|@MAC@|$mac|g" \
      -e "s|^wpa_psk=.*|wpa_psk=$psk|" "$root/harness/$f" > "$tmp/$f"
  done
  python3 -I "$root/scripts/make-wps-cred.py" "$(sed -n 's/^ssid=//p' "$tmp/hostapd-normal.conf")" "$psk" \
    "$tmp/normal.cred" > /dev/null || fail "internal error (pairing credential)"
  echo "identity: $ap_mac, symbols $syms, key written"
  set -x
  ip link set "$ifc" down; ip link set "$ifc" address "$ap_mac"; ip link set "$ifc" up

  # 3. Pair: the pairing AP with the PIN armed, channel by channel, until the pad registers (or 5 min, or cancel).
  gui "STAGE sync $syms"
  local paired=0 deadline=$((SECONDS + 300)) c
  while [ $paired = 0 ] && [ $SECONDS -lt $deadline ]; do
    for c in $PAIR_CHANNELS; do
      case " $chans " in *" $c "*) ;; *) continue;; esac
      sed "s|^channel=.*|channel=$c|" "$tmp/hostapd-pairing.conf" > "$tmp/pair.conf"
      "$hostapd" -dd -t "$tmp/pair.conf" > "$tmp/pair-ch$c.log" 2>&1 & hp=$!
      for _ in $(seq 1 30); do
        grep -qE 'AP-ENABLED|AP-DISABLED' "$tmp/pair-ch$c.log" && break; kill -0 "$hp" 2>/dev/null || break; sleep 0.5
      done
      if grep -q AP-ENABLED "$tmp/pair-ch$c.log"; then
        "$hostapd_cli" -p "$tmp/hostapd" -i "$ifc" wps_pin any "$pin"
        wait_or_cancel 60 'WPS-REG-SUCCESS|WPS-SUCCESS' "$tmp/pair-ch$c.log" || { stop_ap; fail "cancelled"; }
        grep -qE 'WPS-REG-SUCCESS|WPS-SUCCESS' "$tmp/pair-ch$c.log" && paired=1
        sleep 2   # let the pad take the credential before the AP goes
      fi
      # event names only: hostapd -dd also prints "WPS: Network Key" hexdumps, which must not reach the log
      grep -E 'AP-ENABLED|WPS-(REG-)?SUCCESS|WPS-FAIL|WPS-PIN-NEEDED|WPS-TIMEOUT|AP-STA-(CONNECTED|DISCONNECTED)|Could not|Unable' \
        "$tmp/pair-ch$c.log" | grep -v -i 'key'
      stop_ap
      [ $paired = 1 ] && break
      [ $SECONDS -lt $deadline ] || break
    done
  done
  [ $paired = 1 ] || fail "the GamePad didn't sync. Press SYNC on the GamePad and enter the symbols, then try again"

  # 4. Install: the network service, this pairing, the adapter.
  gui "STAGE finish"
  "$root/scripts/install-system.sh" --no-start || fail "couldn't install the GamePad service"
  mkdir -p "$etc"
  { set +x; } 2>/dev/null
  umask 077
  printf 'AP_MAC=%s\nPSK=%s\nADAPTER_MAC=%s\n' "$ap_mac" "$psk" "$perm" > "$etc/pad.conf"
  umask 022
  set -x
  printf 'NAME=%s\nADAPTER_MAC=%s\n' "$name" "$perm" > "$etc/setup"
  # start the network whenever this adapter appears (by its factory address)
  printf '# Written by GamePad setup: start the GamePad network when this adapter appears.\nACTION!="remove", SUBSYSTEM=="net", ATTR{address}=="%s", ATTR{type}=="1", TAG+="systemd", ENV{SYSTEMD_WANTS}+="cemu-gamepad-net.service"\n' \
    "$perm" > /etc/udev/rules.d/70-cemu-gamepad.rules
  udevadm control --reload
  done_ok=1
  gui "RESULT works"
}

# Undo setup: the GamePad network off, the pairing and adapter choice forgotten, the adapter back to the system.
cmd_reset() {
  [ "$(id -u)" -eq 0 ] || { echo "run as root: sudo $0 reset" >&2; exit 1; }
  local d p adapter=""
  [ -r "$etc/setup" ] && adapter=$(sed -n 's/^ADAPTER_MAC=//p' "$etc/setup")
  systemctl stop cemu-gamepad-net.service 2>/dev/null
  rm -rf "$etc" /etc/udev/rules.d/70-cemu-gamepad.rules /run/udev/rules.d/71-cemu-gamepad-tsf.rules
  udevadm control --reload
  for d in /sys/class/net/*; do   # the adapter setup used: plain station, factory address, NetworkManager's again
    [ -e "$d/phy80211" ] || continue
    p=$(perm_mac "$(basename "$d")")
    [ -z "$adapter" ] || [ "$p" = "$adapter" ] || continue
    [ -n "$adapter" ] || [ "$(basename "$(readlink -f "$d/device/driver")")" = rt2800usb ] || continue
    ip link set "$(basename "$d")" down; iw dev "$(basename "$d")" set type managed
    ip link set "$(basename "$d")" address "$p"; ip link set "$(basename "$d")" up
    nmcli device set "$(basename "$d")" managed yes
  done
  echo "GamePad setup reset. Open Cemu > Settings > GamePad > Set up GamePad to start over."
}

case ${1:-} in
  list) cmd_list ;;
  reset) cmd_reset ;;
  status) cmd_status ;;
  setup) [ -n "${2:-}" ] || { echo "usage: $0 setup IFACE" >&2; exit 2; }; cmd_setup "$2" ;;
  install-user) cmd_install_user ;;
  *) echo "usage: $0 list | status | setup IFACE | install-user | reset" >&2; exit 2 ;;
esac
