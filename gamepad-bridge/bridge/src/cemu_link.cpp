#include "cemu_link.h"
#include "clock.h"
#include "log.h"

#include <cstdio>
#include <cstring>

namespace drcb {

CemuLink::CemuLink(EventLoop& loop, const std::string& socket_path, uint32_t max_w, uint32_t max_h)
	: m_server(loop, socket_path, "cemu link"), m_max_w(max_w), m_max_h(max_h)
{
	m_server.on_message = [this](const Message& m) { handle(m); };
	m_server.on_disconnect = [this] {
		m_slots.reset();
		if (m_ready)
		{
			m_ready = false;
			LOGI("cemu detached");
			if (on_attach_changed)
				on_attach_changed(false);
		}
	};
}

bool CemuLink::start() { return m_server.listen(); }

void CemuLink::handle(const Message& msg)
{
	switch (msg.header.type)
	{
	case DRCB_MSG_HELLO: {
		auto* hello = msg.as<drcb_hello>();
		if (!hello || hello->version_major != DRCB_VERSION_MAJOR)
		{
			drcb_reject r{DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR, DRCB_REJECT_VERSION, {}};
			snprintf(r.text, sizeof(r.text), "GamePad bridge speaks IPC %d.%d, Cemu speaks %d.x", DRCB_VERSION_MAJOR,
					 DRCB_VERSION_MINOR, hello ? hello->version_major : -1);
			send_msg(m_server.client(), DRCB_MSG_REJECT, r);
			LOGE("cemu: %s", r.text);
			m_server.drop_client();
			return;
		}
		const uint32_t slot_size = m_max_w * m_max_h * 4;
		if (!m_slots.create("drcb-cemu-frames", kSlots, slot_size))
		{
			drcb_reject r{DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR, DRCB_REJECT_INTERNAL, "bridge could not allocate frame memory"};
			send_msg(m_server.client(), DRCB_MSG_REJECT, r);
			m_server.drop_client();
			return;
		}
		drcb_welcome w{DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR, m_pad_connected, uint32_t(m_pad_source), kSlots,
					   slot_size, m_max_w, m_max_h};
		if (!send_msg(m_server.client(), DRCB_MSG_WELCOME, w, m_slots.fd()))
		{
			m_server.drop_client();
			return;
		}
		m_ready = true;
		char name[sizeof(hello->client_name) + 1]{};
		memcpy(name, hello->client_name, sizeof(hello->client_name));
		LOGI("cemu attached: \"%s\" pid %u, IPC %u.%u", name, hello->pid, hello->version_major, hello->version_minor);
		if (on_attach_changed)
			on_attach_changed(true);
		break;
	}
	case DRCB_MSG_FRAME_SUBMIT: {
		const int64_t t_rx = now_ns();
		auto* s = msg.as<drcb_frame_submit>();
		if (!m_ready || !s)
			return;
		const uint64_t needed = uint64_t(s->stride) * s->height;
		if (!m_slots.valid(s->slot) || s->format != DRCB_FMT_RGBA8 || s->width > m_max_w || s->height > m_max_h ||
			s->stride < s->width * 4 || needed > m_slots.slot_size())
		{
			LOGE("cemu: invalid FRAME_SUBMIT (slot %u, %ux%u stride %u, format %u); dropping client", s->slot, s->width,
				 s->height, s->stride, s->format);
			m_server.drop_client();
			return;
		}
		if (on_frame)
			on_frame(*s, m_slots.slot(s->slot), t_rx);
		drcb_frame_release r{s->slot, 0};
		send_msg(m_server.client(), DRCB_MSG_FRAME_RELEASE, r);
		break;
	}
	case DRCB_MSG_GOODBYE:
		m_server.drop_client();
		break;
	default:
		LOGD("cemu: ignoring message type %u", msg.header.type);
	}
}

void CemuLink::send_presented(uint64_t cemu_frame_id, int64_t t_presented_ns, uint32_t flags)
{
	if (m_ready)
		send_msg(m_server.client(), DRCB_MSG_FRAME_PRESENTED, drcb_frame_presented{cemu_frame_id, t_presented_ns, flags, 0});
}

void CemuLink::send_input(const drcb_input_state& s)
{
	if (m_ready)
		send_msg(m_server.client(), DRCB_MSG_INPUT_STATE, s);
}

void CemuLink::send_pad_status(bool connected, drcb_pad_source source, int battery_percent)
{
	m_pad_connected = connected;
	m_pad_source = source;
	if (m_ready)
		send_msg(m_server.client(), DRCB_MSG_PAD_STATUS, drcb_pad_status{connected, uint32_t(source), battery_percent, 0});
}

void CemuLink::shutdown()
{
	if (m_server.has_client())
	{
		send_msg(m_server.client(), DRCB_MSG_GOODBYE, drcb_goodbye{DRCB_BYE_SHUTDOWN, 0});
		m_server.drop_client();
	}
}

} // namespace drcb
