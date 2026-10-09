# cemu-gamepad-bridge

The daemon between Cemu and the real Wii U GamePad: it receives the GamePad picture from Cemu, encodes it the way a
Wii U does and sends it over the GamePad's Wi-Fi network, and passes the GamePad's input back to Cemu.
How it talks to Cemu: `../docs/IPC.md`. How it talks to the GamePad: `../docs/PROTOCOL.md`.

Built by `../build.sh`. Development tools it also builds: `mock-pad` (a window standing in for the GamePad),
`fake-cemu` (synthetic frames), `tsf-probe` (reads the Wi-Fi clock the way the bridge does; the setup test uses it).
