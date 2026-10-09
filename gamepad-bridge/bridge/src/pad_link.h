#pragma once
// Where frames for the GamePad go and where its input comes from.
// Implementations: MockPadLink (a window, for development) and later the real libdrc link.
#include "drcbridge/ipc.h"

#include <cstdint>
#include <functional>

namespace drcb {

// Frame size the pad's encoder takes: libdrc include/drc/screen.h:31-32 (docs/PROTOCOL.md).
constexpr uint32_t kPadWidth = 864;
constexpr uint32_t kPadHeight = 480;

struct EncodedFrame;

struct PadFrameView
{
	const uint8_t* rgba; // kPadWidth x kPadHeight, stride kPadWidth * 4
	uint64_t pad_frame_id;
	int64_t t_ready_ns; // when the bridge finished preparing it (after encode, if encoded)
	const EncodedFrame* encoded = nullptr; // the pad's video for this frame; links that want it use it
};

class PadLink
{
public:
	virtual ~PadLink() = default;
	virtual const char* name() const = 0;
	virtual drcb_pad_source source() const = 0;
	virtual bool connected() const = 0;

	// Whether submit() would accept a frame right now. Check BEFORE encoding: an encoded P frame can't be
	// dropped afterwards without breaking the pad's reference chain until the next IDR.
	virtual bool can_accept() const = 0;
	// Returns false if the frame was dropped (no pad, or the pad still holds every slot). Never blocks.
	virtual bool submit(const PadFrameView& frame) = 0;
	// Whether the bridge should encode frames for this link (false: the link encodes, or doesn't need video).
	virtual bool wants_encoded() const = 0;
	// The pad's battery: VanillaBatteryStatus 0-6 (HidExtra::battery_level), or -1 if this link doesn't know.
	virtual int battery_level() const { return -1; }

	std::function<void(uint64_t pad_frame_id, int64_t t_presented_ns, uint32_t flags)> on_presented;
	std::function<void(const drcb_input_state&)> on_input;
	std::function<void(bool connected)> on_connection_changed;
};

} // namespace drcb
