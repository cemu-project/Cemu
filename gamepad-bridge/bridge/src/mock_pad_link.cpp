#include "mock_pad_link.h"
#include "clock.h"
#include "drc_encoder.h"
#include "log.h"

#include <cstdio>
#include <cstring>

namespace drcb {

MockPadLink::MockPadLink(EventLoop& loop, const std::string& socket_path)
	: m_server(loop, socket_path, "mock-pad link")
{
	m_server.on_message = [this](const Message& m) { handle(m); };
	m_server.on_disconnect = [this] {
		m_slots.reset();
		if (m_ready)
		{
			m_ready = false;
			if (on_connection_changed)
				on_connection_changed(false);
		}
	};
}

bool MockPadLink::start() { return m_server.listen(); }

void MockPadLink::handle(const Message& msg)
{
	switch (msg.header.type)
	{
	case DRCB_MSG_HELLO: {
		auto* hello = msg.as<drcb_hello>();
		if (!hello || hello->version_major != DRCB_VERSION_MAJOR)
		{
			drcb_reject r{DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR, DRCB_REJECT_VERSION, {}};
			snprintf(r.text, sizeof(r.text), "bridge speaks %d.%d", DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR);
			send_msg(m_server.client(), DRCB_MSG_REJECT, r);
			LOGE("mock pad: version mismatch (got %d), rejected", hello ? hello->version_major : -1);
			m_server.drop_client();
			return;
		}
		const uint32_t slot_size = kPadWidth * kPadHeight * 4;
		if (!m_slots.create("drcb-pad-frames", kSlots, slot_size))
		{
			m_server.drop_client();
			return;
		}
		drcb_welcome w{DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR, 1, DRCB_PAD_MOCK, kSlots, slot_size, kPadWidth, kPadHeight};
		if (!send_msg(m_server.client(), DRCB_MSG_WELCOME, w, m_slots.fd()))
		{
			m_server.drop_client();
			return;
		}
		m_ready = true;
		LOGI("mock pad attached (pid %u)", hello->pid);
		if (on_connection_changed)
			on_connection_changed(true);
		break;
	}
	case DRCB_MSG_FRAME_RELEASE:
		if (auto* r = msg.as<drcb_frame_release>(); r && m_slots.valid(r->slot))
			m_slots.set_in_use(r->slot, false);
		break;
	case DRCB_MSG_FRAME_PRESENTED:
		if (auto* p = msg.as<drcb_frame_presented>(); p && on_presented)
			on_presented(p->frame_id, p->t_presented_ns, p->flags);
		break;
	case DRCB_MSG_INPUT_STATE:
		if (auto* s = msg.as<drcb_input_state>(); s && on_input)
			on_input(*s);
		break;
	case DRCB_MSG_GOODBYE:
		m_server.drop_client();
		break;
	default:
		LOGD("mock pad: ignoring message type %u", msg.header.type);
	}
}

bool MockPadLink::submit(const PadFrameView& frame)
{
	if (!m_ready)
		return false;
	const int slot = m_slots.find_free();
	if (slot < 0)
		return false; // pad still holds every slot: drop rather than queue
	drcb_frame_submit s{};
	s.slot = uint32_t(slot);
	s.frame_id = frame.pad_frame_id;
	s.width = kPadWidth;
	s.height = kPadHeight;
	uint8_t* dst = m_slots.slot(uint32_t(slot));
	if (frame.encoded)
	{
		// The real pad stream: the mock pad has to decode it, like the GamePad would.
		const EncodedFrame& e = *frame.encoded;
		drcb_encoded_frame h{};
		h.idr = e.idr;
		for (int i = 0; i < DRCB_DRC_CHUNKS; i++)
			h.chunk_size[i] = e.chunk_size[i];
		if (sizeof(h) + e.data.size() > m_slots.slot_size())
		{
			LOGE("encoded frame of %zu bytes doesn't fit a pad slot", e.data.size());
			return false;
		}
		memcpy(dst, &h, sizeof(h));
		memcpy(dst + sizeof(h), e.data.data(), e.data.size());
		s.format = DRCB_FMT_DRC_H264;
		s.stride = uint32_t(sizeof(h) + e.data.size());
	}
	else
	{
		memcpy(dst, frame.rgba, size_t(kPadWidth) * kPadHeight * 4);
		s.format = DRCB_FMT_RGBA8;
		s.stride = kPadWidth * 4;
	}
	m_slots.set_in_use(uint32_t(slot), true);
	s.t_flip_ns = frame.t_ready_ns;
	s.t_submit_ns = now_ns();
	if (!send_msg(m_server.client(), DRCB_MSG_FRAME_SUBMIT, s))
	{
		m_slots.set_in_use(uint32_t(slot), false);
		return false;
	}
	return true;
}

} // namespace drcb
