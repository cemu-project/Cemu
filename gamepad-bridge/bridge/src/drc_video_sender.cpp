#include "drc_video_sender.h"
#include "clock.h"
#include "log.h"
#include "pad_link.h"

#include <drc/internal/astrm-packet.h>
#include <drc/internal/tsf.h>
extern "C" {
#include <x264.h>
}

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace drcb {

namespace {

constexpr int kChunks = 5;                      // libdrc kH264ChunksPerFrame; sc-vstrm.rst
constexpr int kMbPerFrame = (kPadWidth / 16) * (kPadHeight / 16);
constexpr int kMbPerChunk = kMbPerFrame / kChunks;
constexpr size_t kVstrmHeader = 16;             // 8-byte header + 8 bytes of options (vstrm-packet.cpp)
constexpr int kMaxPayloadLimit = 2047;          // 11-bit payload_size field (sc-vstrm.rst)

uint32_t tsf32()
{
	drc::u64 tsf = 0;
	drc::GetTsf(&tsf);
	return uint32_t(tsf & 0xFFFFFFFF); // libdrc video-streamer.cpp GetTimestamp
}

int env_int(const char* k, int def) { const char* v = getenv(k); return v && *v ? atoi(v) : def; }

} // namespace

DrcVideoOptions DrcVideoSender::options_from_env()
{
	// DRCB_VIDEO_PRESET, _RC (cqp|crf), _QP, _CRF, _QPMIN, _QPMAX, _MAXKB, _PAYLOAD, _GAPUS
	DrcVideoOptions o;
	if (const char* v = getenv("DRCB_VIDEO_PRESET")) o.preset = v;
	if (const char* v = getenv("DRCB_VIDEO_RC")) o.rc = v;
	if (const char* v = getenv("DRCB_VIDEO_CRF")) o.crf = float(atof(v));
	o.qp = env_int("DRCB_VIDEO_QP", o.qp);
	o.qp_min = env_int("DRCB_VIDEO_QPMIN", o.qp_min);
	o.qp_max = env_int("DRCB_VIDEO_QPMAX", o.qp_max);
	o.max_frame_kb = env_int("DRCB_VIDEO_MAXKB", o.max_frame_kb);
	o.max_payload = std::min(kMaxPayloadLimit, std::max(256, env_int("DRCB_VIDEO_PAYLOAD", o.max_payload)));
	o.packet_gap_us = env_int("DRCB_VIDEO_GAPUS", o.packet_gap_us);
	o.spread_us = env_int("DRCB_VIDEO_SPREADUS", o.spread_us);
	o.vbv_kb = env_int("DRCB_VIDEO_VBVKB", o.vbv_kb);
	o.chunk_slices = env_int("DRCB_VIDEO_CHUNKSLICES", 0) != 0;
	o.i4x4 = env_int("DRCB_VIDEO_I4X4", 1) != 0;
	o.i16x16_mask = env_int("DRCB_VIDEO_I16MASK", 0);
	o.chroma_mask = env_int("DRCB_VIDEO_CHROMAMASK", 0);
	o.no_i16x16 = env_int("DRCB_VIDEO_NOI16", 0) != 0;
	o.mv_range_x = env_int("DRCB_VIDEO_MVX", o.mv_range_x);
	o.mv_range_y = env_int("DRCB_VIDEO_MVY", o.mv_range_y);
	if (const char* v = getenv("DRCB_VIDEO_IDRCRF")) o.idr_crf_offset = float(atof(v));
	o.ts_offset_us = env_int("DRCB_VIDEO_TSOFF", o.ts_offset_us);
	o.lead_us = env_int("DRCB_VIDEO_LEADUS", o.lead_us);
	o.sweep = env_int("DRCB_VIDEO_SWEEP", 0) != 0;
	o.aq = env_int("DRCB_VIDEO_AQ", 1) != 0;
	if (const char* v = getenv("DRCB_VIDEO_IPF")) o.ip_factor = float(atof(v));
	if (const char* v = getenv("DRCB_VIDEO_RESYNC")) o.resync = v;
	if (const char* v = getenv("DRCB_VIDEO_SEND")) o.send_mode = v;
	if (const char* v = getenv("DRCB_VIDEO_PACING")) o.pacing = v;
	o.bind_50020 = env_int("DRCB_VIDEO_BIND50020", 1) != 0;
	o.resync_holdoff_ms = env_int("DRCB_VIDEO_HOLDOFF", o.resync_holdoff_ms);
	return o;
}

namespace {
// DRCB_VIDEO_SWEEP phases. The first one is libdrc's encoder exactly (h264-encoder.cpp: slow, QP 32), so every
// difference from libdrc can be judged by the pad's resync rate against it.
struct SweepPhase { const char* name; int spread_us; const char* preset; };
// Pad-side tearing (our frames decode clean, runs/20261009-112308-padnet; later timestamps are rejected,
// runs/20261009-111956-play): the pad draws a frame while it arrives, so the last chunks have to arrive sooner.
// 40 s each; flick the pad in every stretch and note which tears least.
const SweepPhase kPhases[] = {
	{"A packets spread over 10 ms (current)", 10000, "veryfast"},
	{"B all packets right after encoding", 0, "veryfast"},
	{"C all packets right after encoding, fastest encoder", 0, "ultrafast"},
	{"D packets spread over 4 ms", 4000, "veryfast"},
};
} // namespace

struct DrcVideoSender::Impl
{
	DrcVideoOptions opt;
	x264_t* enc = nullptr;
	int fd = -1;          // the one in use
	int fd_bound = -1;    // 192.168.1.10:50020
	int fd_any = -1;      // ephemeral, like libdrc's UdpClient
	std::vector<std::vector<uint8_t>> held; // send_mode "next": packets waiting for the next frame
	std::vector<bool> held_is_astrm;
	std::vector<int> held_chunk; // "console": the chunk each held packet belongs to (astrm: chunk 0)
	int cur_chunk = 0;
	// "console" mode: packets go out from their own thread at their due time, so frame N's last chunks
	// (timestamp + 17 ms) can still be on their way while frame N+1 is being encoded.
	struct Sched { int64_t due_ns; int fd; bool astrm; std::vector<uint8_t> d; };
	std::deque<Sched> sq;
	std::mutex smtx;
	std::condition_variable scv;
	bool squit = false;
	std::thread sthread;
	std::atomic<uint64_t> late_chunks{0}; // chunk bursts that left more than 1 ms after their due time
	void schedule_console();
	void run_sender();
	void xmit(const uint8_t* d, size_t n, bool astrm_pkt);
	void flush_held();
	void send_paced();
	sockaddr_in vid_dst{}, aud_dst{};

	std::thread thread;
	std::mutex mtx;
	std::condition_variable cv;
	std::vector<uint8_t> pending, encoding;
	bool have_pending = false, quit = false;
	int64_t t_pending = 0;
	std::atomic<bool> idr_requested{false};
	std::atomic<bool> active{true}; // set_active
	int64_t last_recovery_ns = 0;
	uint32_t tick_ts = 0; // pacing "clock": this tick's grid timestamp
	uint64_t ticks_skipped = 0;
	bool console_idr = false; // pacing clock: next encode is an IDR (console-style resync)
	int skip_ticks = 0;       // pacing clock: slots to leave empty
	uint64_t ticks_gap = 0;   // slots left empty on purpose (resync)
	void run_clock();
	uint64_t resyncs_acted = 0;

	// per-frame state, encoder thread only
	bool inited = false;
	uint16_t seq = 0;
	uint32_t timestamp = 0;
	bool frame_idr = false;
	int chunks_sent = 0;
	int64_t t_submit = 0;
	DrcVideoStats stats;
	std::vector<uint8_t> pkt;
	drc::AstrmPacket astrm;

	void (*on_frame)(void*, const DrcVideoStats&) = nullptr;
	void* on_frame_user = nullptr;

	bool open_encoder();
	void run();
	void encode_one();
	void send_chunk(int idx, const uint8_t* data, size_t size);
	static void nalu_trampoline(x264_t*, x264_nal_t* nal, void* opaque)
	{
		static_cast<Impl*>(opaque)->on_nal(nal);
	}
	void on_nal(x264_nal_t* nal);
	void flush_chunk(int idx, const uint8_t* data, size_t size);
	// drc-x264 only cuts a chunk once the CABAC output has grown, so a fully static region (e.g. a repeated
	// frame) can fold one chunk into the next: NALs for chunks 0,1,3,4 only. The pad gets 5 chunks per frame
	// (sc-vstrm.rst) and decodes them as ONE concatenated CABAC stream (vanilla video.c joins every packet), so
	// the byte cut points are free: spread the bytes over the missing chunk numbers instead of dropping any.
	// (Dropping them truncated frames: video-sender-test, clock pacing, "bytestream -6". libdrc keeps the
	// previous frame's pointer for a missing chunk and sends stale bytes.)
	void flush_span(int first, int last, const uint8_t* data, size_t size);
	int pending_chunk = -1;
	size_t pending_size = 0;
	const uint8_t* pending_ptr = nullptr;

	// percentiles every kStatsFrames frames (CLAUDE.md: percentiles, not averages)
	const int kStatsFrames = env_int("DRCB_VIDEO_STATSFRAMES", 600);
	std::vector<double> s_enc, s_first, s_last, s_bytes, s_crf;
	float stats_crf = 0;
	uint64_t s_resyncs0 = 0;
	int s_idr = 0;
	void log_stats();

	// DRCB_VIDEO_SWEEP: which setting makes the pad reject frames? One phase per kPhaseNs, resync rate logged.
	int64_t phase_ns = int64_t(env_int("DRCB_VIDEO_SWEEPSEC", 40)) * 1'000'000'000;
	int phase = -1;
	int64_t phase_start = 0;
	std::atomic<uint64_t> resyncs{0};
	uint64_t phase_resyncs0 = 0;
	std::vector<double> ph_bytes, ph_last;
	void sweep_tick();
	std::atomic<bool> libdrc_turn{false};
	const std::atomic<uint64_t>* input_counter = nullptr;
	uint64_t phase_input0 = 0;

	// Frame-size control without VBV: raise the CRF quickly when a frame goes over max_frame_kb, lower it
	// slowly back to opt.crf when frames are well under. x264_encoder_reconfig changes it for the next frame.
	float crf_now = -1;
	void crf_control();
};

void DrcVideoSender::Impl::crf_control()
{
	if (opt.rc != "crf")
		return;
	if (crf_now < 0)
		crf_now = opt.crf;
	const double limit = opt.max_frame_kb * 1024.0;
	float next = crf_now;
	if (stats.frame_bytes > limit)
		next = std::min(51.0f, crf_now + 3.0f);
	else if (stats.frame_bytes > 0.75 * limit)
		next = std::min(51.0f, crf_now + 1.0f);
	else if (stats.frame_bytes < 0.4 * limit && crf_now > opt.crf)
		next = std::max(opt.crf, crf_now - 0.25f);
	if (next != crf_now)
	{
		x264_param_t p;
		x264_encoder_parameters(enc, &p);
		p.rc.f_rf_constant = next;
		x264_encoder_reconfig(enc, &p);
		crf_now = next;
	}
	stats_crf = crf_now;
}



void DrcVideoSender::Impl::sweep_tick()
{
	const int64_t now = now_ns();
	ph_bytes.push_back(stats.frame_bytes);
	ph_last.push_back(stats.last_chunk_ns / 1e6);
	if (phase >= 0 && now - phase_start < phase_ns)
		return;
	if (phase >= 0)
	{
		std::sort(ph_bytes.begin(), ph_bytes.end());
		std::sort(ph_last.begin(), ph_last.end());
		const double secs = (now - phase_start) / 1e9;
		LOGI("video SWEEP result [%s]: resyncs %.1f/s, pad input %.0f packets/s, frame bytes p50 %.0f p95 %.0f, last chunk sent p50 %.2f ms (%zu frames)",
			 kPhases[phase].name, (resyncs - phase_resyncs0) / secs, input_counter ? (*input_counter - phase_input0) / secs : -1.0, ph_bytes[ph_bytes.size() / 2],
			 ph_bytes[ph_bytes.size() * 95 / 100], ph_last[ph_last.size() / 2], ph_bytes.size());
	}
	phase = (phase + 1) % int(sizeof(kPhases) / sizeof(kPhases[0]));
	const SweepPhase& p = kPhases[phase];
	opt.spread_us = p.spread_us;
	opt.preset = p.preset;
	libdrc_turn = false;
	flush_held();
	x264_encoder_close(enc);
	enc = nullptr;
	open_encoder();
	crf_now = -1;
	inited = false; // next frame is an IDR with the init flag, as after a fresh start
	phase_start = now;
	phase_resyncs0 = resyncs;
	phase_input0 = input_counter ? input_counter->load() : 0;
	ph_bytes.clear();
	ph_last.clear();
	LOGI("video SWEEP phase %s", p.name);
}

void DrcVideoSender::Impl::log_stats()
{
	s_enc.push_back(stats.encode_ns / 1e6);
	s_first.push_back(stats.first_chunk_ns / 1e6);
	s_last.push_back(stats.last_chunk_ns / 1e6);
	s_bytes.push_back(stats.frame_bytes);
	s_crf.push_back(stats_crf);
	s_idr += stats.idr;
	if (int(s_enc.size()) < kStatsFrames)
		return;
	auto p = [](std::vector<double>& v, double q) {
		std::sort(v.begin(), v.end());
		return v[std::min(v.size() - 1, size_t(q * v.size()))];
	};
	LOGI("video stats %d frames: encode ms p50 %.2f p95 %.2f p99 %.2f | submit->first chunk sent p50 %.2f p99 %.2f | "
		 "->last chunk sent p50 %.2f p95 %.2f p99 %.2f | frame bytes p50 %.0f p95 %.0f p99 %.0f max %.0f | IDR %d | crf p50 %.1f max %.1f | resync requests %llu, recoveries %llu (%s) | slots left empty %llu | ticks skipped %llu | chunks late >1 ms %llu",
		 kStatsFrames, p(s_enc, .5), p(s_enc, .95), p(s_enc, .99), p(s_first, .5), p(s_first, .99), p(s_last, .5),
		 p(s_last, .95), p(s_last, .99), p(s_bytes, .5), p(s_bytes, .95), p(s_bytes, .99), p(s_bytes, 1.0), s_idr, p(s_crf, .5), p(s_crf, 1.0), (unsigned long long)(resyncs - s_resyncs0), (unsigned long long)resyncs_acted, opt.resync.c_str(), (unsigned long long)ticks_gap, (unsigned long long)ticks_skipped, (unsigned long long)late_chunks.exchange(0));
	ticks_gap = 0;
	s_resyncs0 = resyncs;
	resyncs_acted = 0;
	ticks_skipped = 0;
	s_enc.clear(); s_first.clear(); s_last.clear(); s_bytes.clear(); s_crf.clear(); s_idr = 0;
}

bool DrcVideoSender::Impl::open_encoder()
{
	// libdrc H264Encoder::CreateEncoder, transcribed; the rate control is the one switchable difference.
	x264_param_t param;
	if (x264_param_default_preset(&param, opt.preset.c_str(), "zerolatency") < 0)
	{
		LOGE("video: unknown x264 preset '%s'", opt.preset.c_str());
		return false;
	}
	param.i_width = kPadWidth;
	param.i_height = kPadHeight;
	param.i_fps_num = 60000; // 59.94 Hz (vstrm frame rate 0); x264's VBV needs the real frame rate
	param.i_fps_den = 1001;
	param.b_vfr_input = 0;
	param.analyse.inter &= ~X264_ANALYSE_PSUB16x16;
	param.i_keyint_min = 10; // kEnableIntraRefresh
	param.i_keyint_max = 30;
	param.i_scenecut_threshold = -1;
	param.i_csp = X264_CSP_I420;
	param.b_cabac = 1;
	param.b_interlaced = 0;
	param.i_bframe = 0;
	param.i_bframe_pyramid = 0;
	param.i_frame_reference = 1;
	param.b_constrained_intra = 1;
	param.b_intra_refresh = 1;
	param.analyse.i_weighted_pred = 0;
	param.analyse.b_weighted_bipred = 0;
	param.analyse.b_transform_8x8 = 0;
	param.analyse.i_chroma_qp_offset = 0;
	if (opt.rc == "crf")
	{
		// Per-macroblock QP, as the console does. In DRH mode every frame's (implicit) slice QP stays 32
		// (drc-x264 encoder.c: sh.i_qp = 32, pps i_pic_init_qp = 32); x264 codes the difference as mb_qp_delta.
		param.rc.i_rc_method = X264_RC_CRF;
		param.rc.f_rf_constant = opt.crf;
		param.rc.i_qp_min = opt.qp_min;
		param.rc.i_qp_max = opt.qp_max;
		param.rc.i_aq_mode = opt.aq ? X264_AQ_VARIANCE : X264_AQ_NONE;
		// No VBV: x264's VBV re-encodes macroblock rows that overshoot (bitstream restored to the row start),
		// AFTER the chunk before them has gone out, so the pad got corrupted frames exactly when frames were big
		// (runs/20261008-212054-play: frames at the 24 KB cap, ~50 resyncs/s; video-sender-test with a tight cap:
		// decode errors at chunk boundaries). The frame size is held by crf_control() between frames instead.
	}
	else
	{
		param.rc.i_rc_method = X264_RC_CQP;
		param.rc.i_qp_constant = param.rc.i_qp_min = param.rc.i_qp_max = opt.qp;
	}
	param.rc.f_ip_factor = opt.ip_factor; // libdrc: 1.0
	// VBV (x264 hard frame cap) corrupts DRH chunks even when sent after the encode: its row re-encodes restart
	// a chunk's NAL mid-chunk (video-sender-test, busy content: decode errors at 8 and 4 KB caps). Kept for
	// experiments only.
	if (opt.vbv_kb > 0 && opt.send_mode != "progressive")
	{
		// Hard per-frame cap, IDRs included. Row-level VBV keeps the 5 chunks of a frame roughly even.
		if (param.rc.i_rc_method == X264_RC_CQP)
		{
			param.rc.i_rc_method = X264_RC_CRF; // VBV needs a rate-controlled mode; CRF ~ the fixed QP
			param.rc.f_rf_constant = float(opt.qp) - 3.0f;
			param.rc.i_qp_min = 10;
			param.rc.i_qp_max = 51;
			param.rc.i_aq_mode = X264_AQ_NONE;
		}
		param.rc.i_vbv_buffer_size = opt.vbv_kb * 8;
		param.rc.i_vbv_max_bitrate = int(opt.vbv_kb * 8 * 59.94);
		param.rc.f_vbv_buffer_init = 1.0f;
	}
	param.b_repeat_headers = 0;
	param.b_aud = 0;
	param.b_drh_mode = 1;
	param.b_drh_chunk_slices = opt.chunk_slices ? 1 : 0;
	param.i_drh_i16x16_mask = opt.i16x16_mask;
	param.i_drh_chroma_mask = opt.chroma_mask;
	param.b_drh_no_i16x16 = opt.no_i16x16 ? 1 : 0;
	param.i_drh_mv_range_x = opt.mv_range_x;
	param.i_drh_mv_range_y = opt.mv_range_y;
	if (!opt.i4x4)
	{
		param.analyse.intra &= ~X264_ANALYSE_I4x4;
		param.analyse.inter &= ~X264_ANALYSE_I4x4;
	}
	param.i_threads = 1;
	param.b_sliced_threads = 0;
	param.i_slice_count = 1;
	param.nalu_process = &Impl::nalu_trampoline;
	param.i_log_level = X264_LOG_WARNING;
	x264_param_apply_profile(&param, "main");
	enc = x264_encoder_open(&param);
	if (!enc)
	{
		LOGE("video: x264_encoder_open failed");
		return false;
	}
	LOGI("video: own sender: MVs within %dx%d px, no-i16x16 %s, chunk slices %s, preset %s, rc %s (%s), resync %s (holdoff %d ms), send %s (spread / timestamp lead %d us) from %s, payload %d B, packet gap %d us", opt.mv_range_x, opt.mv_range_y, opt.no_i16x16 ? "on" : "off", opt.chunk_slices ? "on" : "off", opt.preset.c_str(), opt.rc.c_str(),
		 opt.rc == "crf" ? ("crf " + std::to_string(int(opt.crf)) + " qp " + std::to_string(opt.qp_min) + "-" +
							std::to_string(opt.qp_max) + ", frames kept under " + std::to_string(opt.max_frame_kb) + " KB").c_str()
						 : ("qp " + std::to_string(opt.qp)).c_str(),
		 opt.resync.c_str(), opt.resync_holdoff_ms, opt.send_mode.c_str(), opt.send_mode == "console" ? opt.lead_us : opt.spread_us, opt.bind_50020 ? "port 50020" : "an ephemeral port", opt.max_payload, opt.packet_gap_us);
	return true;
}

void DrcVideoSender::Impl::send_chunk(int idx, const uint8_t* data, size_t size)
{
	// libdrc GenerateVstrmPackets + VstrmPacket, for one chunk, sent right away.
	const bool init = !inited;
	bool first = true;
	do
	{
		const size_t n = std::min(size, size_t(opt.max_payload));
		const bool last = (n == size);
		pkt.assign(kVstrmHeader + n, 0);
		pkt[0] = uint8_t(0xF0 | ((seq >> 8) & 3)); // magic F, type 0, seq id high bits
		pkt[1] = uint8_t(seq & 0xFF);
		seq = (seq + 1) % 1024;
		pkt[2] = uint8_t((init ? 0x80 : 0) | ((idx == 0 && first) ? 0x40 : 0) | (last ? 0x20 : 0) |
						 ((idx == kChunks - 1 && last) ? 0x10 : 0) | 0x08 /* has timestamp */ | ((n >> 8) & 7));
		pkt[3] = uint8_t(n & 0xFF);
		pkt[4] = uint8_t(timestamp >> 24);
		pkt[5] = uint8_t(timestamp >> 16);
		pkt[6] = uint8_t(timestamp >> 8);
		pkt[7] = uint8_t(timestamp);
		// options, in libdrc's order: 0x83 force decoding, 0x85 6 MB rows, [0x80 IDR], 0x82 frame rate 0 (59.94)
		size_t o = 8;
		pkt[o++] = 0x83;
		pkt[o++] = 0x85;
		pkt[o++] = 6;
		if (frame_idr)
			pkt[o++] = 0x80;
		pkt[o++] = 0x82;
		pkt[o++] = 0;
		memcpy(pkt.data() + kVstrmHeader, data, n);
		xmit(pkt.data(), pkt.size(), false);
		stats.packets++;
		data += n;
		size -= n;
		first = false;
		if (size && opt.packet_gap_us > 0)
			std::this_thread::sleep_for(std::chrono::microseconds(opt.packet_gap_us));
	} while (size);
}

void DrcVideoSender::Impl::on_nal(x264_nal_t* nal)
{
	// libdrc H264Encoder::ProcessNalUnit: one NAL per chunk in DRH mode, SEI ignored.
	if (nal->i_type == NAL_SEI || opt.send_mode != "progressive")
		return; // "after"/"next": sent from x264's final NAL list once the encode has returned
	const int idx = nal->i_first_mb / kMbPerChunk;
	if (idx < 0 || idx >= kChunks)
		return;
	// A chunk is a cut of ONE continuous CABAC bitstream (drc-x264 encoder.c "emulate Wii U DRH chunking"), and
	// a CABAC carry can still change the last byte already written when the next bytes go out. So a chunk is
	// final only once the next chunk exists: send chunk k-1 here, from the bytes right before chunk k (the
	// chunks are contiguous), and the last chunk after x264_encoder_encode returns. Sending each chunk the
	// moment it appeared corrupted the rows after every chunk boundary (video-sender-test: ffmpeg errors at MB
	// rows 6, 12, ...; libdrc reads all chunks after the encode and has none).
	if (pending_chunk >= 0 && idx > pending_chunk)
		flush_span(pending_chunk, idx - 1, nal->p_payload - pending_size, pending_size);
	pending_chunk = idx;
	pending_size = size_t(nal->i_payload);
	pending_ptr = nal->p_payload;
}

void DrcVideoSender::Impl::flush_span(int first, int last, const uint8_t* data, size_t size)
{
	const int n = last - first + 1;
	for (int i = 0; i < n; i++)
	{
		// the first chunk takes everything but one byte for each later one (fewer if there aren't enough)
		const size_t later = size_t(n - 1 - i);
		size_t take = (i == n - 1) ? size : (size > later ? (i == 0 ? size - later : 1) : (size ? 1 : 0));
		take = std::min(take, size);
		flush_chunk(first + i, data, take);
		data += take;
		size -= take;
	}
}

void DrcVideoSender::Impl::flush_chunk(int idx, const uint8_t* data, size_t size)
{
	cur_chunk = idx;
	if (chunks_sent == 0)
	{
		// libdrc sends the astrm "video format" packet right before each frame's vstrm packets
		uint8_t payload[24] = {0, 0, 0, 0, 0x80, 0x3e, 0, 0, 0x80, 0x3e, 0, 0, 0x80, 0x3e, 0, 0, 0x80, 0x3e, 0, 0,
							   0, 0, 0, 0};
		payload[0] = uint8_t(timestamp);
		payload[1] = uint8_t(timestamp >> 8);
		payload[2] = uint8_t(timestamp >> 16);
		payload[3] = uint8_t(timestamp >> 24);
		astrm.ResetPacket();
		astrm.SetPacketType(drc::AstrmPacketType::kVideoFormat);
		astrm.SetTimestamp(0x00100000);
		astrm.SetPayload(payload, sizeof(payload));
		xmit(astrm.GetBytes(), astrm.GetSize(), true);
	}
	send_chunk(idx, data, size);
	stats.frame_bytes += uint32_t(size);
	const int64_t now = now_ns();
	if (chunks_sent == 0)
		stats.first_chunk_ns = now - t_submit;
	stats.last_chunk_ns = now - t_submit;
	chunks_sent++;
}

void DrcVideoSender::Impl::xmit(const uint8_t* d, size_t n, bool astrm_pkt)
{
	if (opt.send_mode == "next" || opt.send_mode == "console" || (opt.send_mode == "after" && opt.spread_us > 0))
	{
		held.emplace_back(d, d + n);
		held_is_astrm.push_back(astrm_pkt);
		held_chunk.push_back(cur_chunk);
		return;
	}
	const sockaddr_in& dst = astrm_pkt ? aud_dst : vid_dst;
	sendto(fd, d, n, 0, reinterpret_cast<const sockaddr*>(&dst), sizeof(dst));
}

void DrcVideoSender::Impl::send_paced()
{
	// Evenly over spread_us, never past the next tick: first packet now, last at +spread (or sooner if the
	// encode already ate into the slot).
	const size_t n = held.size();
	if (!n)
		return;
	const int64_t t0 = now_ns();
	int64_t window = int64_t(opt.spread_us) * 1000;
	const int64_t slot_left = int64_t(14'000'000) - (t0 - t_submit); // leave ~2.7 ms of the 16.7 ms tick
	window = std::max<int64_t>(0, std::min(window, slot_left));
	for (size_t i = 0; i < n; i++)
	{
		if (n > 1 && window > 0)
		{
			const int64_t due = t0 + window * int64_t(i) / int64_t(n - 1);
			const int64_t wait = due - now_ns();
			if (wait > 20'000)
				std::this_thread::sleep_for(std::chrono::nanoseconds(wait));
		}
		const sockaddr_in& dst = held_is_astrm[i] ? aud_dst : vid_dst;
		sendto(fd, held[i].data(), held[i].size(), 0, reinterpret_cast<const sockaddr*>(&dst), sizeof(dst));
	}
	held.clear();
	held_is_astrm.clear();
	held_chunk.clear();
	stats.last_chunk_ns = now_ns() - t_submit;
}

namespace {
// The real Wii U: when each chunk's burst leaves, us after the frame's vstrm timestamp, on the console's clock
// (scripts/console-timing.py on runs/20261009-114650-resync: chunk ends p50 6.13 / 9.10 / 11.95 / 14.90 /
// 17.22 ms; each chunk is a ~0.1 ms burst).
constexpr int kConsoleChunkUs[5] = {6000, 8950, 11800, 14750, 17100};
}

void DrcVideoSender::Impl::schedule_console()
{
	if (held.empty())
		return;
	drc::u64 tsf = 0;
	drc::GetTsf(&tsf);
	const int64_t now = now_ns();
	{
		std::lock_guard<std::mutex> lk(smtx);
		for (size_t i = 0; i < held.size(); i++)
		{
			const int k = std::min(4, std::max(0, held_chunk[i]));
			const int32_t rel_us = int32_t(timestamp + uint32_t(kConsoleChunkUs[k]) - uint32_t(tsf));
			sq.push_back(Sched{now + int64_t(rel_us) * 1000, fd, bool(held_is_astrm[i]), std::move(held[i])});
		}
	}
	scv.notify_all();
	held.clear();
	held_is_astrm.clear();
	held_chunk.clear();
	stats.last_chunk_ns = now_ns() - t_submit; // queued (sent later, at the console's times)
}

void DrcVideoSender::Impl::run_sender()
{
	std::unique_lock<std::mutex> lk(smtx);
	int64_t burst_due = -1;
	for (;;)
	{
		scv.wait(lk, [&] { return squit || !sq.empty(); });
		if (squit)
			return;
		const int64_t wait = sq.front().due_ns - now_ns();
		if (wait > 20'000)
		{
			scv.wait_for(lk, std::chrono::nanoseconds(wait), [&] { return squit; });
			continue;
		}
		Sched p = std::move(sq.front());
		sq.pop_front();
		lk.unlock();
		if (p.due_ns != burst_due)
		{
			burst_due = p.due_ns;
			if (now_ns() - p.due_ns > 1'000'000)
				late_chunks++;
		}
		const sockaddr_in& dst = p.astrm ? aud_dst : vid_dst;
		sendto(p.fd, p.d.data(), p.d.size(), 0, reinterpret_cast<const sockaddr*>(&dst), sizeof(dst));
		lk.lock();
	}
}

void DrcVideoSender::Impl::flush_held()
{
	for (size_t i = 0; i < held.size(); i++)
	{
		const sockaddr_in& dst = held_is_astrm[i] ? aud_dst : vid_dst;
		sendto(fd, held[i].data(), held[i].size(), 0, reinterpret_cast<const sockaddr*>(&dst), sizeof(dst));
	}
	held.clear();
	held_is_astrm.clear();
}

void DrcVideoSender::Impl::encode_one()
{
	fd = opt.bind_50020 && fd_bound >= 0 ? fd_bound : fd_any;
	if (opt.sweep && libdrc_turn)
	{
		// libdrc's VideoStreamer has this frame (LibdrcPadLink::submit); just keep the sweep clock running
		stats = DrcVideoStats{};
		sweep_tick();
		return;
	}
	flush_held(); // send_mode "next": the previous frame goes out now, like libdrc's timer
	x264_picture_t in;
	x264_picture_init(&in);
	in.img.i_csp = X264_CSP_I420;
	in.img.i_plane = 3;
	in.img.i_stride[0] = kPadWidth;
	in.img.plane[0] = encoding.data();
	in.img.i_stride[1] = kPadWidth / 2;
	in.img.plane[1] = in.img.plane[0] + kPadWidth * kPadHeight;
	in.img.i_stride[2] = kPadWidth / 2;
	in.img.plane[2] = in.img.plane[1] + kPadWidth * kPadHeight / 4;
	frame_idr = !inited || console_idr;
	console_idr = false;
	if (opt.pacing != "clock" && idr_requested.exchange(false))
	{
		const int64_t t = now_ns();
		if (t - last_recovery_ns >= int64_t(opt.resync_holdoff_ms) * 1'000'000)
		{
			last_recovery_ns = t;
			resyncs_acted++;
			if (opt.resync == "idr")
				frame_idr = true;
			else
				x264_encoder_intra_refresh(enc); // starts with this frame (not called during an encode)
		}
	}
	in.i_type = frame_idr ? X264_TYPE_IDR : X264_TYPE_P;
	in.opaque = this;
	timestamp = (opt.pacing == "clock" ? tick_ts : tsf32()) + uint32_t(opt.ts_offset_us) +
				uint32_t(opt.send_mode == "console" ? opt.lead_us : 0);
	chunks_sent = 0;
	pending_chunk = -1;
	stats = DrcVideoStats{};
	stats.idr = frame_idr;
	x264_nal_t* nals;
	int n_nals;
	x264_picture_t out;
	// CRF IDRs a few steps coarser (console IDRs: 27-54 KB; a full-quality CRF IDR of a busy screen can be far
	// bigger and take the pad longer than the two slots it gets). Intra refresh sharpens it again within ~0.5 s.
	const bool coarser_idr = frame_idr && opt.rc == "crf" && opt.idr_crf_offset > 0 && crf_now >= 0;
	if (coarser_idr)
	{
		x264_param_t p;
		x264_encoder_parameters(enc, &p);
		p.rc.f_rf_constant = std::min(51.0f, crf_now + opt.idr_crf_offset);
		x264_encoder_reconfig(enc, &p);
	}
	const int64_t t0 = now_ns();
	x264_encoder_encode(enc, &nals, &n_nals, &in, &out);
	if (coarser_idr)
	{
		x264_param_t p;
		x264_encoder_parameters(enc, &p);
		p.rc.f_rf_constant = crf_now;
		x264_encoder_reconfig(enc, &p);
	}
	stats.encode_ns = now_ns() - t0;
	if (opt.send_mode != "progressive")
	{
		// x264's own NAL list after the encode: every byte final (no CABAC carry or outstanding bytes left),
		// pointers valid even if the bitstream buffer was reallocated. One NAL per chunk, except where a static
		// region folded chunks together (flush_span spreads those).
		// One NAL per chunk index, the LAST one: when VBV re-encodes rows, x264 rewinds the bitstream but its NAL
		// list keeps the superseded NAL too (video-sender-test with a frame cap: frames of 6-8 chunks).
		const x264_nal_t* by_idx[kChunks] = {};
		for (int i = 0; i < n_nals; i++)
		{
			if (nals[i].i_type == NAL_SEI)
				continue;
			const int idx = std::min(kChunks - 1, nals[i].i_first_mb / kMbPerChunk);
			by_idx[idx] = &nals[i];
		}
		// Chunks with no NAL of their own (a static region folded into the next) get bytes spread from the NAL
		// before them, as in flush_span.
		int i = 0;
		while (i < kChunks)
		{
			if (!by_idx[i]) // only possible for chunk 0 if x264 emitted nothing at all
			{
				flush_chunk(i, nullptr, 0);
				i++;
				continue;
			}
			int last = i;
			while (last + 1 < kChunks && !by_idx[last + 1])
				last++;
			flush_span(i, last, by_idx[i]->p_payload, size_t(by_idx[i]->i_payload));
			i = last + 1;
		}
	}
	else if (pending_chunk >= 0) // progressive: the last chunk is final once the encode has returned
		flush_span(pending_chunk, kChunks - 1, pending_ptr, pending_size);
	if (opt.send_mode == "after" && opt.spread_us > 0)
		send_paced();
	else if (opt.send_mode == "console")
		schedule_console();
	if (chunks_sent != kChunks)
		LOGW("video: frame produced %d chunks, expected %d", chunks_sent, kChunks);
	inited = true;
	log_stats();
	crf_control();
	if (opt.sweep)
		sweep_tick();
	if (on_frame)
		on_frame(on_frame_user, stats);
}

void DrcVideoSender::Impl::run()
{
	if (opt.pacing == "clock")
		return run_clock();
	for (;;)
	{
		{
			std::unique_lock<std::mutex> lk(mtx);
			cv.wait(lk, [&] { return quit || have_pending; });
			if (quit)
				return;
			encoding.swap(pending);
			have_pending = false;
			t_submit = t_pending;
		}
		encode_one();
	}
}

void DrcVideoSender::Impl::run_clock()
{
	// The grid lives on the TSF (what the pad displays by); sleeping uses CLOCK_MONOTONIC, re-aimed at the TSF
	// every tick, so the two clocks' drift never accumulates. libdrc video-streamer.cpp does the same with
	// next_timestamp += 1000000/59.94 and a timer re-armed from the TSF.
	constexpr double kPeriodUs = 1000000.0 / (60000.0 / 1001.0); // 16683.35
	double grid = 0;   // next tick, TSF us (64-bit, before wrapping to the 32-bit vstrm field)
	bool have_frame = false;
	for (;;)
	{
		drc::u64 now_tsf = 0;
		drc::GetTsf(&now_tsf);
		if (grid == 0 || double(now_tsf) - grid > 5 * kPeriodUs || grid - double(now_tsf) > 5 * kPeriodUs)
			grid = double(now_tsf); // start, or the TSF jumped (pad reconnect, TSF reset): re-anchor
		// Running late (a slow encode): skip ticks already past instead of sending frames stamped in the past,
		// which the pad drops. The console never stamps a frame earlier than its slot.
		int skipped = 0;
		while (double(now_tsf) - grid > kPeriodUs / 2)
		{
			grid += kPeriodUs;
			skipped++;
		}
		ticks_skipped += skipped;
		const double wait_us = grid - double(now_tsf);
		{
			std::unique_lock<std::mutex> lk(mtx);
			if (wait_us > 0)
				cv.wait_for(lk, std::chrono::microseconds(int64_t(wait_us)), [&] { return quit; });
			if (quit)
				return;
			if (have_pending)
			{
				encoding.swap(pending);
				have_pending = false;
				have_frame = true;
			}
			t_submit = now_ns();
		}
		// Resync the way a real Wii U does it (runs/20261009-104908-resync, 21 forced resyncs, every one the same):
		// the slot after the request stays empty, the next slot carries one IDR (27-44 KB, QP mostly 12-26), and
		// the slot after the IDR stays empty too (timestamp steps of 33367 us on both sides), so the pad has two
		// frame periods to decode the big IDR. One request was always enough. We used to send the IDR and the
		// next P frame in consecutive slots, and the pad kept asking.
		if (!active.load())
		{
			// No GamePad on the network: don't spend a CPU core encoding for nobody. It gets a fresh start
			// (init IDR) when it's back.
			inited = false;
			skip_ticks = 0;
			console_idr = false;
			idr_requested = false;
			grid += kPeriodUs;
			continue;
		}
		bool skip = false;
		if (skip_ticks > 0)
		{
			skip_ticks--;
			skip = true;
		}
		else if (have_frame && opt.resync == "idr" && idr_requested.exchange(false))
		{
			const int64_t t = now_ns();
			if (t - last_recovery_ns >= int64_t(opt.resync_holdoff_ms) * 1'000'000)
			{
				last_recovery_ns = t;
				resyncs_acted++;
				console_idr = true; // IDR in the next slot
				skip = true;        // this slot stays empty
			}
		}
		if (have_frame && !skip) // repeat the last frame when Cemu has nothing new: the console sends every tick
		{
			tick_ts = uint32_t(uint64_t(grid) & 0xFFFFFFFF);
			encode_one();
			if (stats.idr && inited)
				skip_ticks = 1; // the slot after an IDR stays empty
		}
		else if (skip)
			ticks_gap++;
		grid += kPeriodUs;
	}
}

void DrcVideoSender::set_active(bool a) { m->active = a; }

DrcVideoSender::DrcVideoSender(const DrcVideoOptions& opt) : m(new Impl) { m->opt = opt; }
DrcVideoSender::~DrcVideoSender() { stop(); }

bool DrcVideoSender::start()
{
	m->fd_any = socket(AF_INET, SOCK_DGRAM, 0);
	m->fd_bound = socket(AF_INET, SOCK_DGRAM, 0);
	if (m->fd_any < 0 || m->fd_bound < 0)
		return false;
	sockaddr_in src{};
	src.sin_family = AF_INET;
	src.sin_port = htons(50020);
	inet_pton(AF_INET, "192.168.1.10", &src.sin_addr);
	if (bind(m->fd_bound, reinterpret_cast<sockaddr*>(&src), sizeof(src)) != 0)
	{
		if (m->opt.bind_50020)
			LOGW("video: can't bind 192.168.1.10:50020 (%s); sending from an ephemeral port like libdrc", strerror(errno));
		close(m->fd_bound);
		m->fd_bound = -1;
	}
	int sndbuf = 4 << 20;
	setsockopt(m->fd_any, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
	if (m->fd_bound >= 0)
		setsockopt(m->fd_bound, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
	m->fd = m->opt.bind_50020 && m->fd_bound >= 0 ? m->fd_bound : m->fd_any;
	m->vid_dst.sin_family = AF_INET;
	m->vid_dst.sin_port = htons(50120); // libdrc Streamer::kDefaultVideoDest
	inet_pton(AF_INET, m->opt.dest_ip.c_str(), &m->vid_dst.sin_addr);
	m->aud_dst = m->vid_dst;
	m->aud_dst.sin_port = htons(50121); // kDefaultAudioDest
	if (!m->open_encoder())
		return false;
	m->thread = std::thread([this] { m->run(); });
	m->sthread = std::thread([this] { m->run_sender(); });
	return true;
}

void DrcVideoSender::stop()
{
	if (m->thread.joinable())
	{
		{
			std::lock_guard<std::mutex> lk(m->mtx);
			m->quit = true;
		}
		m->cv.notify_all();
		m->thread.join();
	}
	if (m->sthread.joinable())
	{
		{
			std::lock_guard<std::mutex> lk(m->smtx);
			m->squit = true;
		}
		m->scv.notify_all();
		m->sthread.join();
	}
	if (m->enc)
	{
		x264_encoder_close(m->enc);
		m->enc = nullptr;
	}
	for (int* f : {&m->fd_any, &m->fd_bound})
		if (*f >= 0)
		{
			close(*f);
			*f = -1;
		}
	m->fd = -1;
}

void DrcVideoSender::submit(const uint8_t* yuv, size_t size)
{
	{
		std::lock_guard<std::mutex> lk(m->mtx);
		m->pending.assign(yuv, yuv + size);
		m->have_pending = true;
		m->t_pending = now_ns();
	}
	m->cv.notify_one();
}

bool DrcVideoSender::libdrc_turn() const { return m->libdrc_turn; }
void DrcVideoSender::set_input_counter(const std::atomic<uint64_t>* c) { m->input_counter = c; }

void DrcVideoSender::request_idr()
{
	m->idr_requested = true;
	m->resyncs++;
}

void DrcVideoSender::set_on_frame(void (*cb)(void*, const DrcVideoStats&), void* user)
{
	m->on_frame = cb;
	m->on_frame_user = user;
}

} // namespace drcb
