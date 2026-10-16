#pragma once

// Remote Skylander portal sharing.
//
// Lets a friend (e.g. joining through Steam Remote Play Together) place their own
// Skylanders on this Cemu instance's emulated portal over the network.
//
// Wire protocol (TCP, one byte message type, then payload):
//   client -> host
//     'H' <u8 len> <password bytes>     hello / authenticate (must be first)
//     'L' <u8 tag> <1024 bytes figure>  place figure; tag is echoed back
//     'U' <u8 slot>                     remove figure from portal slot
//     'P'                               ping
//   host -> client
//     'h' <u8 ok>                       auth result (connection closed if 0)
//     'l' <u8 tag> <u8 slot>            figure placed in slot (0xFF = portal full)
//     'u' <u8 slot> <u8 ok>             figure removed
//     'D' <u8 slot> <1024 bytes>        figure data changed by the game (save it!)
//     'p'                               pong

#include <cstdint>
#include <string>

namespace nsyshid::SkylanderRemote
{
	constexpr uint16_t DEFAULT_PORT = 27420;

	// Starts listening on the given port. Returns false and fills error on failure.
	bool Start(uint16_t port, const std::string& password, std::string& error);
	// Stops listening, disconnects everyone and takes their figures off the portal.
	void Stop();
	bool IsRunning();
	// Human-readable status line for the GUI.
	std::string GetStatus();
} // namespace nsyshid::SkylanderRemote
