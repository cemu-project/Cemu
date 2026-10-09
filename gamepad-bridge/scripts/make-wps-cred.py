#!/usr/bin/env python3
"""Build the binary WPS Credential attribute for hostapd's extra_cred (console-free pairing).

When the pairing AP (open, WPS) registers the GamePad, hostapd would normally hand out the pairing AP's own
(open) network. With skip_cred_build=1 + extra_cred=<this file> it hands out the NORMAL-mode network instead:
SSID + WPA2-PSK key, which is what the pad needs to join afterwards (libdrc web/docs/re/wifi.rst:34-35).

Layout transcribed from hostapd's registrar (third_party/drc-hostap-vanilla/src/wps/wps_registrar.c,
wps_build_credential / wps_build_credential_wrap, lines ~1523-1615); attribute ids and values from
src/wps/wps_defs.h. Each attribute: u16 type, u16 length (big-endian), value.
  Credential (0x100e) {
    Network Index (0x1026) = 1
    SSID (0x1045)
    Authentication Type (0x1003) = WPA2PSK 0x0020
    Encryption Type (0x100f) = AES 0x0008
    Network Key (0x1027) = the PSK as 64 hex characters (as hostapd does for a PSK, wpa_snprintf_hex)
    MAC Address (0x1020) = enrollee MAC; hostapd uses the pad's, a static file can't know it -> zeros.
                           TODO: verify the real pad accepts that (POST-HW).
  }

usage: make-wps-cred.py SSID PSK_HEX64 OUT.cred
"""
import struct
import sys

ATTR_CRED = 0x100E
ATTR_NETWORK_INDEX = 0x1026
ATTR_SSID = 0x1045
ATTR_AUTH_TYPE = 0x1003
ATTR_ENCR_TYPE = 0x100F
ATTR_NETWORK_KEY = 0x1027
ATTR_MAC_ADDR = 0x1020
WPS_AUTH_WPA2PSK = 0x0020
WPS_ENCR_AES = 0x0008


def tlv(t, value):
    return struct.pack(">HH", t, len(value)) + value


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    ssid, psk_hex, out = sys.argv[1].encode(), sys.argv[2].lower(), sys.argv[3]
    if len(psk_hex) != 64 or any(c not in "0123456789abcdef" for c in psk_hex):
        print("PSK must be 64 hex characters (the wpa_psk value from the normal-mode hostapd config)")
        return 2
    if not 1 <= len(ssid) <= 32:
        print("SSID must be 1-32 bytes")
        return 2
    inner = (tlv(ATTR_NETWORK_INDEX, b"\x01")
             + tlv(ATTR_SSID, ssid)
             + tlv(ATTR_AUTH_TYPE, struct.pack(">H", WPS_AUTH_WPA2PSK))
             + tlv(ATTR_ENCR_TYPE, struct.pack(">H", WPS_ENCR_AES))
             + tlv(ATTR_NETWORK_KEY, psk_hex.encode())
             + tlv(ATTR_MAC_ADDR, b"\x00" * 6))
    with open(out, "wb") as f:
        f.write(tlv(ATTR_CRED, inner))
    print(f"wrote {out}: {4 + len(inner)} bytes, SSID {ssid.decode()!r}, WPA2-PSK/AES")
    return 0


if __name__ == "__main__":
    sys.exit(main())
