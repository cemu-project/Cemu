// VERBATIM COPY of cemu-gamepad project bridge/include/drcbridge/ipc.h. Do not edit here; update both.
// Cemu <-> GamePad bridge IPC, version 1.0. Normative; docs/IPC.md explains it.
// Self-contained on purpose: the Cemu fork carries a verbatim copy of this file.
// Plain C-compatible structs, host byte order (same machine), CLOCK_MONOTONIC nanoseconds.
#pragma once

#include <stdint.h>

#define DRCB_MAGIC 0x42435244u /* "DRCB" little-endian */
#define DRCB_VERSION_MAJOR 1
#define DRCB_VERSION_MINOR 0

#define DRCB_SOCKET_DIR "cemu-gamepad" /* under $XDG_RUNTIME_DIR */
#define DRCB_SOCKET_NAME "bridge.sock"

#define DRCB_MAX_PACKET 4096

enum drcb_msg_type
{
	DRCB_MSG_HELLO = 1,
	DRCB_MSG_WELCOME = 2,
	DRCB_MSG_REJECT = 3,
	DRCB_MSG_FRAME_SUBMIT = 10,
	DRCB_MSG_FRAME_RELEASE = 11,
	DRCB_MSG_FRAME_PRESENTED = 12,
	DRCB_MSG_INPUT_STATE = 20,
	DRCB_MSG_PAD_STATUS = 40,
	DRCB_MSG_GOODBYE = 99,
};

struct drcb_header
{
	uint32_t magic;
	uint16_t version_major;
	uint16_t type;
	uint32_t payload_size;
	uint32_t reserved;
};

struct drcb_hello
{
	uint16_t version_major;
	uint16_t version_minor;
	uint32_t pid;
	char client_name[32];
};

enum drcb_pad_source
{
	DRCB_PAD_NONE = 0,
	DRCB_PAD_MOCK = 1,
	DRCB_PAD_REAL = 2,
};

// WELCOME arrives with exactly one memfd in SCM_RIGHTS: slot_count * slot_size bytes.
struct drcb_welcome
{
	uint16_t version_major;
	uint16_t version_minor;
	uint32_t pad_connected;
	uint32_t pad_source; // drcb_pad_source
	uint32_t slot_count;
	uint32_t slot_size;
	uint32_t max_width;
	uint32_t max_height;
};

enum drcb_reject_reason
{
	DRCB_REJECT_VERSION = 1,
	DRCB_REJECT_BUSY = 2,
	DRCB_REJECT_INTERNAL = 3,
};

struct drcb_reject
{
	uint16_t version_major;
	uint16_t version_minor;
	uint32_t reason; // drcb_reject_reason
	char text[128];
};

enum drcb_pixel_format
{
	DRCB_FMT_RGBA8 = 1,
};

struct drcb_frame_submit
{
	uint32_t slot;
	uint32_t format; // drcb_pixel_format
	uint64_t frame_id; // monotonically increasing per connection
	uint32_t width;
	uint32_t height;
	uint32_t stride; // bytes per row
	uint32_t reserved;
	int64_t t_flip_ns; // Cemu saw the DRC flip
	int64_t t_submit_ns; // pixels are in the slot
};

struct drcb_frame_release
{
	uint32_t slot;
	uint32_t reserved;
};

#define DRCB_PRESENTED_ESTIMATED 0x1u // t_presented is an estimate, not a measurement

struct drcb_frame_presented
{
	uint64_t frame_id;
	int64_t t_presented_ns;
	uint32_t flags;
	uint32_t reserved;
};

// Logical buttons. Bridge-defined; the Cemu side maps these to VPAD.
#define DRCB_BTN_A (1u << 0)
#define DRCB_BTN_B (1u << 1)
#define DRCB_BTN_X (1u << 2)
#define DRCB_BTN_Y (1u << 3)
#define DRCB_BTN_LEFT (1u << 4)
#define DRCB_BTN_RIGHT (1u << 5)
#define DRCB_BTN_UP (1u << 6)
#define DRCB_BTN_DOWN (1u << 7)
#define DRCB_BTN_ZL (1u << 8)
#define DRCB_BTN_ZR (1u << 9)
#define DRCB_BTN_L (1u << 10)
#define DRCB_BTN_R (1u << 11)
#define DRCB_BTN_PLUS (1u << 12)
#define DRCB_BTN_MINUS (1u << 13)
#define DRCB_BTN_HOME (1u << 14)
#define DRCB_BTN_STICK_L (1u << 15)
#define DRCB_BTN_STICK_R (1u << 16)
#define DRCB_BTN_TV (1u << 17)
#define DRCB_BTN_POWER (1u << 18)

struct drcb_input_state
{
	uint64_t seq;
	int64_t t_received_ns;
	uint32_t buttons; // DRCB_BTN_*
	uint32_t touch_down;
	float stick_l[2]; // x, y in [-1, 1], +y up
	float stick_r[2];
	float touch[2]; // x, y in [0, 1], origin top-left
	float accel[3]; // g
	float gyro[3]; // degrees per second
};

struct drcb_pad_status
{
	uint32_t connected;
	uint32_t source; // drcb_pad_source
	int32_t battery_percent; // -1 = unknown
	uint32_t reserved;
};

enum drcb_goodbye_reason
{
	DRCB_BYE_NORMAL = 0,
	DRCB_BYE_SHUTDOWN = 1,
	DRCB_BYE_PROTOCOL_ERROR = 2,
};

struct drcb_goodbye
{
	uint32_t reason;
	uint32_t reserved;
};

#ifdef __cplusplus
static_assert(sizeof(drcb_header) == 16, "drcb_header layout");
static_assert(sizeof(drcb_frame_submit) == 48, "drcb_frame_submit layout");
static_assert(sizeof(drcb_input_state) == 72, "drcb_input_state layout");
#endif
