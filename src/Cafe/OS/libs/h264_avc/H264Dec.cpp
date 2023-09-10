#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/OS/libs/h264_avc/parser/H264Parser.h"
#include "util/highresolutiontimer/HighResolutionTimer.h"
#include "Cafe/CafeSystem.h"

#include "h264dec.h"

extern "C"
{
#include "../dependencies/ih264d/common/ih264_typedefs.h"
#include "../dependencies/ih264d/decoder/ih264d.h"
};

enum class H264DEC_STATUS : uint32
{
	SUCCESS = 0x0,
	BAD_STREAM = 0x1000000,
	INVALID_PARAM = 0x1010000,
};

namespace H264
{
	bool H264_IsBotW()
	{
		// Cemuhook has a hack where it always returns a small size for H264DECMemoryRequirement (256 bytes)
		// it also outputs images pre-cropped instead of giving the game raw uncropped images
		// both of these are required to allow Breath of the Wild to playback the higher res (1080p) videos from the Switch version
		// we mirror these hacks for user convenience and because there are no downsides
		uint64 currentTitleId = CafeSystem::GetForegroundTitleId();
		if (currentTitleId == 0x00050000101c9500 || currentTitleId == 0x00050000101c9400 || currentTitleId == 0x00050000101c9300)
			return true;
		return false;
	}

	uint32 H264DECMemoryRequirement(uint32 codecProfile, uint32 codecLevel, uint32 width, uint32 height, uint32be* sizeRequirementOut)
	{
		if (H264_IsBotW())
		{
			*sizeRequirementOut = 256;
			return 0;
		}

		// note: On console this seems to check if maxWidth or maxHeight < 64 but Pikmin 3 passes 32x32 and crashes if this function fails ?
		if (width < 0x20 || height < 0x20 || width > 2800 || height > 1408 || sizeRequirementOut == MPTR_NULL || codecLevel >= 52 || (codecProfile != 0x42 && codecProfile != 0x4D && codecProfile != 0x64))
			return 0x1010000;

		uint32 workbufferSize = 0;
		if (codecLevel < 0xB)
		{
			workbufferSize = 0x18C << 10;
		}
		else if (codecLevel == 0xB)
		{
			workbufferSize = 0x384 << 10;
		}
		else if (codecLevel >= 0xC && codecLevel <= 0x14)
		{
			workbufferSize = 0x948 << 10;
		}
		else if (codecLevel == 0x15)
		{
			workbufferSize = 0x1290 << 10;
		}
		else if (codecLevel >= 0x16 && codecLevel <= 0x1E)
		{
			workbufferSize = 0x1FA4 << 10;
		}
		else if (codecLevel == 0x1F)
		{
			workbufferSize = 0x4650 << 10;
		}
		else if (codecLevel == 0x20)
		{
			workbufferSize = 0x1400000;
		}
		else if (codecLevel >= 0x21 && codecLevel <= 0x29)
		{
			workbufferSize = 0x8000 << 10;
		}
		else if (codecLevel == 0x2A)
		{
			workbufferSize = 0x2200000;
		}
		else if (codecLevel >= 0x2B && codecLevel <= 0x32)
		{
			workbufferSize = 0x1AF40 << 10;
		}
		else if (codecLevel >= 0x33)
		{
			workbufferSize = 0x2D000 << 10;
		}
		workbufferSize += 0x447;
		*sizeRequirementOut = workbufferSize;
		return 0;
	}

	uint32 H264DECCheckMemSegmentation(MPTR memory, uint32 size)
	{
		// return 0 for valid, 1 for invalid
		// currently we allow any range
		return 0;
	}

	H264DEC_STATUS H264DECFindDecstartpoint(uint8* ptr, uint32 length, uint32be* offsetOut)
	{
		if (!ptr || length < 4 || !offsetOut)
			return H264DEC_STATUS::INVALID_PARAM;
		for (uint32 i = 0; i < length - 4; ++i)
		{
			uint8 b = ptr[i];
			if (b != 0)
				continue;

			b = ptr[i + 1];
			if (b != 0)
				continue;

			b = ptr[i + 2];
			if (b != 1)
				continue;

			b = ptr[i + 3];
			b &= 0x9F;
			if (b != 7) // check for NAL type SPS
				continue;

			if (i > 0)
				*offsetOut = i - 1;
			else
				*offsetOut = 0;

			return H264DEC_STATUS::SUCCESS;
		}
		return H264DEC_STATUS::BAD_STREAM;
	}

	H264DEC_STATUS H264DECFindIdrpoint(uint8* ptr, uint32 length, uint32be* offsetOut)
	{
		if (!ptr || length < 4 || !offsetOut)
			return H264DEC_STATUS::INVALID_PARAM;

		for (uint32 i = 0; i < length - 4; ++i)
		{
			uint8 b = ptr[i];
			if (b != 0)
				continue;

			b = ptr[i + 1];
			if (b != 0)
				continue;

			b = ptr[i + 2];
			if (b != 1)
				continue;

			b = ptr[i + 3];
			b &= 0x9F;
			if (b != 5 && b != 7 && b != 8) // check for NAL type IDR slice, but also accept SPS or PPS slices
				continue;

			if (i > 0)
				*offsetOut = i - 1;
			else
				*offsetOut = 0;

			return H264DEC_STATUS::SUCCESS;
		}
		return H264DEC_STATUS::BAD_STREAM;
	}

	struct H264Context 
	{
		struct  
		{
			MEMPTR<void> ptr{ nullptr };
			uint32be length{ 0 };
			float64be timestamp;
		}BitStream;
		struct  
		{
			MEMPTR<void> outputFunc{ nullptr };
			uint8be outputPerFrame{ 0 }; // whats the default?
			MEMPTR<void> userMemoryParam{ nullptr };
		}Param;
		// misc
		uint32be sessionHandle;
	};

	class H264AVCDecoder
	{
		static void* ivd_aligned_malloc(void* ctxt, WORD32 alignment, WORD32 size)
		{
#ifdef _WIN32
			return _aligned_malloc(size, alignment);
#else
			// alignment is atleast sizeof(void*)
			alignment = std::max<WORD32>(alignment, sizeof(void*));

			//smallest multiple of 2 at least as large as alignment
			alignment--;
			alignment |= alignment << 1;
			alignment |= alignment >> 1;
			alignment |= alignment >> 2;
			alignment |= alignment >> 4;
			alignment |= alignment >> 8;
			alignment |= alignment >> 16;
			alignment ^= (alignment >> 1);

			void* temp;
			posix_memalign(&temp, (size_t)alignment, (size_t)size);
			return temp;
#endif
		}

		static void ivd_aligned_free(void* ctxt, void* buf) 
		{
#ifdef _WIN32
			_aligned_free(buf);
#else
			free(buf);
#endif
			return;
		}

	public:
		struct DecodeResult
		{
			bool frameReady{ false };
			double timestamp;
			void* imageOutput;
			ivd_video_decode_op_t decodeOutput;
		};

		void Init(bool isBufferedMode)
		{
			ih264d_create_ip_t s_create_ip{ 0 };
			ih264d_create_op_t s_create_op{ 0 };

			s_create_ip.s_ivd_create_ip_t.u4_size = sizeof(ih264d_create_ip_t);
			s_create_ip.s_ivd_create_ip_t.e_cmd = IVD_CMD_CREATE;
			s_create_ip.s_ivd_create_ip_t.u4_share_disp_buf = 1; // shared display buffer mode -> We give the decoder a list of buffers that it will use (?)
			
			s_create_op.s_ivd_create_op_t.u4_size = sizeof(ih264d_create_op_t);
			s_create_ip.s_ivd_create_ip_t.e_output_format = IV_YUV_420SP_UV;
			s_create_ip.s_ivd_create_ip_t.pf_aligned_alloc = ivd_aligned_malloc;
			s_create_ip.s_ivd_create_ip_t.pf_aligned_free = ivd_aligned_free;
			s_create_ip.s_ivd_create_ip_t.pv_mem_ctxt = NULL;

			WORD32 status = ih264d_api_function(m_codecCtx, &s_create_ip, &s_create_op);
			cemu_assert(!status);

			m_codecCtx = (iv_obj_t*)s_create_op.s_ivd_create_op_t.pv_handle;
			m_codecCtx->pv_fxns = (void*)&ih264d_api_function;
			m_codecCtx->u4_size = sizeof(iv_obj_t);

			SetDecoderCoreCount(1);

			m_isBufferedMode = isBufferedMode;

			UpdateParameters(false);

			m_bufferedResults.clear();
			m_numDecodedFrames = 0;
			m_hasBufferSizeInfo = false;
			m_timestampIndex = 0;
		}

		void Destroy()
		{
			if (!m_codecCtx)
				return;
			ih264d_delete_ip_t s_delete_ip{ 0 };
			ih264d_delete_op_t s_delete_op{ 0 };
			s_delete_ip.s_ivd_delete_ip_t.u4_size = sizeof(ih264d_delete_ip_t);
			s_delete_ip.s_ivd_delete_ip_t.e_cmd = IVD_CMD_DELETE;
			s_delete_op.s_ivd_delete_op_t.u4_size = sizeof(ih264d_delete_op_t);
			WORD32 status = ih264d_api_function(m_codecCtx, &s_delete_ip, &s_delete_op);
			cemu_assert_debug(!status);
			m_codecCtx = nullptr;
		}

		void SetDecoderCoreCount(uint32 coreCount)
		{
			ih264d_ctl_set_num_cores_ip_t s_set_cores_ip;
			ih264d_ctl_set_num_cores_op_t s_set_cores_op;
			s_set_cores_ip.e_cmd = IVD_CMD_VIDEO_CTL;
			s_set_cores_ip.e_sub_cmd = (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_SET_NUM_CORES;
			s_set_cores_ip.u4_num_cores = coreCount; // valid numbers are 1-4
			s_set_cores_ip.u4_size = sizeof(ih264d_ctl_set_num_cores_ip_t);
			s_set_cores_op.u4_size = sizeof(ih264d_ctl_set_num_cores_op_t);
			IV_API_CALL_STATUS_T status = ih264d_api_function(m_codecCtx, (void *)&s_set_cores_ip, (void *)&s_set_cores_op);
			cemu_assert(status == IV_SUCCESS);
		}

		static bool GetImageInfo(uint8* stream, uint32 length, uint32& imageWidth, uint32& imageHeight)
		{
			// create temporary decoder
			ih264d_create_ip_t s_create_ip{ 0 };
			ih264d_create_op_t s_create_op{ 0 };
			s_create_ip.s_ivd_create_ip_t.u4_size = sizeof(ih264d_create_ip_t);
			s_create_ip.s_ivd_create_ip_t.e_cmd = IVD_CMD_CREATE;
			s_create_ip.s_ivd_create_ip_t.u4_share_disp_buf = 0;
			s_create_op.s_ivd_create_op_t.u4_size = sizeof(ih264d_create_op_t);
			s_create_ip.s_ivd_create_ip_t.e_output_format = IV_YUV_420SP_UV;
			s_create_ip.s_ivd_create_ip_t.pf_aligned_alloc = ivd_aligned_malloc;
			s_create_ip.s_ivd_create_ip_t.pf_aligned_free = ivd_aligned_free;
			s_create_ip.s_ivd_create_ip_t.pv_mem_ctxt = NULL;
			iv_obj_t* ctx = nullptr;
			WORD32 status = ih264d_api_function(ctx, &s_create_ip, &s_create_op);
			cemu_assert_debug(!status);
			if (status != IV_SUCCESS)
				return false;
			ctx = (iv_obj_t*)s_create_op.s_ivd_create_op_t.pv_handle;
			ctx->pv_fxns = (void*)&ih264d_api_function;
			ctx->u4_size = sizeof(iv_obj_t);
			// set header-only mode
			ih264d_ctl_set_config_ip_t s_h264d_ctl_ip{ 0 };
			ih264d_ctl_set_config_op_t s_h264d_ctl_op{ 0 };
			ivd_ctl_set_config_ip_t* ps_ctl_ip = &s_h264d_ctl_ip.s_ivd_ctl_set_config_ip_t;
			ivd_ctl_set_config_op_t* ps_ctl_op = &s_h264d_ctl_op.s_ivd_ctl_set_config_op_t;
			ps_ctl_ip->u4_disp_wd = 0;
			ps_ctl_ip->e_frm_skip_mode = IVD_SKIP_NONE;
			ps_ctl_ip->e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
			ps_ctl_ip->e_vid_dec_mode = IVD_DECODE_HEADER;
			ps_ctl_ip->e_cmd = IVD_CMD_VIDEO_CTL;
			ps_ctl_ip->e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
			ps_ctl_ip->u4_size = sizeof(ih264d_ctl_set_config_ip_t);
			ps_ctl_op->u4_size = sizeof(ih264d_ctl_set_config_op_t);
			status = ih264d_api_function(ctx, &s_h264d_ctl_ip, &s_h264d_ctl_op);
			cemu_assert(!status);
			// decode stream
			ivd_video_decode_ip_t s_dec_ip{ 0 };
			ivd_video_decode_op_t s_dec_op{ 0 };
			s_dec_ip.u4_size = sizeof(ivd_video_decode_ip_t);
			s_dec_op.u4_size = sizeof(ivd_video_decode_op_t);
			s_dec_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
			s_dec_ip.pv_stream_buffer = stream;
			s_dec_ip.u4_num_Bytes = length;
			s_dec_ip.s_out_buffer.u4_num_bufs = 0;

			s_dec_op.u4_raw_wd = 0;
			s_dec_op.u4_raw_ht = 0;

			status = ih264d_api_function(ctx, &s_dec_ip, &s_dec_op);
			//cemu_assert(status == 0); -> This errors when not both the headers are present, but it will still set the parameters we need
			bool isValid = false;
			if (true)//status == 0)
			{
				imageWidth = s_dec_op.u4_raw_wd;
				imageHeight = s_dec_op.u4_raw_ht;
				cemu_assert_debug(imageWidth != 0 && imageHeight != 0);
				isValid = true;
			}
			// destroy decoder
			ih264d_delete_ip_t s_delete_ip{ 0 };
			ih264d_delete_op_t s_delete_op{ 0 };
			s_delete_ip.s_ivd_delete_ip_t.u4_size = sizeof(ih264d_delete_ip_t);
			s_delete_ip.s_ivd_delete_ip_t.e_cmd = IVD_CMD_DELETE;
			s_delete_op.s_ivd_delete_op_t.u4_size = sizeof(ih264d_delete_op_t);
			status = ih264d_api_function(ctx, &s_delete_ip, &s_delete_op);
			cemu_assert_debug(!status);
			return isValid;
		}

		void Decode(void* data, uint32 length, double timestamp, void* imageOutput, DecodeResult& decodeResult)
		{
			if (!m_hasBufferSizeInfo)
			{
				uint32 numByteConsumed = 0;
				if (!DetermineBufferSizes(data, length, numByteConsumed))
				{
					cemuLog_log(LogType::Force, "H264: Unable to determine picture size. Ignoring decode input");
					decodeResult.frameReady = false;
					return;
				}
				length -= numByteConsumed;
				data = (uint8*)data + numByteConsumed;
				m_hasBufferSizeInfo = true;
			}

			ivd_video_decode_ip_t s_dec_ip{ 0 };
			ivd_video_decode_op_t s_dec_op{ 0 };
			s_dec_ip.u4_size = sizeof(ivd_video_decode_ip_t);
			s_dec_op.u4_size = sizeof(ivd_video_decode_op_t);

			s_dec_ip.e_cmd = IVD_CMD_VIDEO_DECODE;

			// remember timestamp and associated output buffer
			m_timestamps[m_timestampIndex] = timestamp;
			m_imageBuffers[m_timestampIndex] = imageOutput;
			s_dec_ip.u4_ts = m_timestampIndex;
			m_timestampIndex = (m_timestampIndex + 1) % 64;

			s_dec_ip.pv_stream_buffer = (uint8*)data;
			s_dec_ip.u4_num_Bytes = length;

			s_dec_ip.s_out_buffer.u4_min_out_buf_size[0] = 0;
			s_dec_ip.s_out_buffer.u4_min_out_buf_size[1] = 0;
			s_dec_ip.s_out_buffer.u4_num_bufs = 0;

			BenchmarkTimer bt;
			bt.Start();
			WORD32 status = ih264d_api_function(m_codecCtx, &s_dec_ip, &s_dec_op);
			if (status != 0 && (s_dec_op.u4_error_code&0xFF) == IVD_RES_CHANGED)
			{
				// resolution change
				ResetDecoder();
				m_hasBufferSizeInfo = false;
				Decode(data, length, timestamp, imageOutput, decodeResult);
				return;
			}
			else if (status != 0)
			{
				cemuLog_log(LogType::Force, "H264: Failed to decode frame (error 0x{:08x})", status);
				decodeResult.frameReady = false;
				return;
			}

			bt.Stop();
			double decodeTime = bt.GetElapsedMilliseconds();

			cemu_assert(s_dec_op.u4_frame_decoded_flag);
			cemu_assert_debug(s_dec_op.u4_num_bytes_consumed == length);

			cemu_assert_debug(m_isBufferedMode || s_dec_op.u4_output_present); // if buffered mode is disabled, then every input should output a frame (except for partial slices?)

			if (s_dec_op.u4_output_present)
			{
				cemu_assert(s_dec_op.e_output_format == IV_YUV_420SP_UV);
				if (H264_IsBotW())
				{
					if (s_dec_op.s_disp_frm_buf.u4_y_wd == 1920 && s_dec_op.s_disp_frm_buf.u4_y_ht == 1088)
						s_dec_op.s_disp_frm_buf.u4_y_ht = 1080;
				}
				DecodeResult tmpResult;
				tmpResult.frameReady = s_dec_op.u4_output_present != 0;
				tmpResult.timestamp = m_timestamps[s_dec_op.u4_ts];
				tmpResult.imageOutput = m_imageBuffers[s_dec_op.u4_ts];
				tmpResult.decodeOutput = s_dec_op;
				AddBufferedResult(tmpResult);
				// transfer image to PPC output buffer and also correct stride
				bt.Start();
				CopyImageToResultBuffer((uint8*)s_dec_op.s_disp_frm_buf.pv_y_buf, (uint8*)s_dec_op.s_disp_frm_buf.pv_u_buf, (uint8*)m_imageBuffers[s_dec_op.u4_ts], s_dec_op);
				bt.Stop();
				double copyTime = bt.GetElapsedMilliseconds();
				// release buffer
				sint32 bufferId = -1;
				for (size_t i = 0; i < m_displayBuf.size(); i++)
				{
					if (s_dec_op.s_disp_frm_buf.pv_y_buf >= m_displayBuf[i].data() && s_dec_op.s_disp_frm_buf.pv_y_buf < (m_displayBuf[i].data() + m_displayBuf[i].size()))
					{
						bufferId = (sint32)i;
						break;
					}
				}
				cemu_assert_debug(bufferId == s_dec_op.u4_disp_buf_id);
				cemu_assert(bufferId >= 0);
				ivd_rel_display_frame_ip_t s_video_rel_disp_ip{ 0 };
				ivd_rel_display_frame_op_t s_video_rel_disp_op{ 0 };
				s_video_rel_disp_ip.e_cmd = IVD_CMD_REL_DISPLAY_FRAME;
				s_video_rel_disp_ip.u4_size = sizeof(ivd_rel_display_frame_ip_t);
				s_video_rel_disp_op.u4_size = sizeof(ivd_rel_display_frame_op_t);
				s_video_rel_disp_ip.u4_disp_buf_id = bufferId;
				status = ih264d_api_function(m_codecCtx, &s_video_rel_disp_ip, &s_video_rel_disp_op);
				cemu_assert(!status);

				cemuLog_log(LogType::H264, "H264Bench | DecodeTime {}ms CopyTime {}ms", decodeTime, copyTime);
			}
			else
			{
				cemuLog_log(LogType::H264, "H264Bench | DecodeTime{}ms", decodeTime);
			}

			if (s_dec_op.u4_frame_decoded_flag)
				m_numDecodedFrames++;

			if (m_isBufferedMode)
			{
				// in buffered mode, always buffer 5 frames regardless of actual reordering and decoder latency
				if (m_numDecodedFrames > 5)
					GetCurrentBufferedResult(decodeResult);
			}
			else if(m_numDecodedFrames > 0)
				GetCurrentBufferedResult(decodeResult);

			// get VUI
			//ih264d_ctl_get_vui_params_ip_t s_ctl_get_vui_params_ip;
			//ih264d_ctl_get_vui_params_op_t s_ctl_get_vui_params_op;

			//s_ctl_get_vui_params_ip.e_cmd = IVD_CMD_VIDEO_CTL;
			//s_ctl_get_vui_params_ip.e_sub_cmd = (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_GET_VUI_PARAMS;
			//s_ctl_get_vui_params_ip.u4_size = sizeof(ih264d_ctl_get_vui_params_ip_t);
			//s_ctl_get_vui_params_op.u4_size = sizeof(ih264d_ctl_get_vui_params_op_t);

			//status = ih264d_api_function(mCodecCtx, &s_ctl_get_vui_params_ip, &s_ctl_get_vui_params_op);
			//cemu_assert(status == 0);
		}

		std::vector<DecodeResult> Flush()
		{
			std::vector<DecodeResult> results;
			// set flush mode
			ivd_ctl_flush_ip_t s_video_flush_ip{ 0 };
			ivd_ctl_flush_op_t s_video_flush_op{ 0 };
			s_video_flush_ip.e_cmd = IVD_CMD_VIDEO_CTL;
			s_video_flush_ip.e_sub_cmd = IVD_CMD_CTL_FLUSH;
			s_video_flush_ip.u4_size = sizeof(ivd_ctl_flush_ip_t);
			s_video_flush_op.u4_size = sizeof(ivd_ctl_flush_op_t);
			WORD32 status = ih264d_api_function(m_codecCtx, &s_video_flush_ip, &s_video_flush_op);
			if (status != 0)
				cemuLog_log(LogType::Force, "H264Dec: Unexpected error during flush ({})", status);
			// get all frames from the codec
			while (true)
			{
				ivd_video_decode_ip_t s_dec_ip{ 0 };
				ivd_video_decode_op_t s_dec_op{ 0 };
				s_dec_ip.u4_size = sizeof(ivd_video_decode_ip_t);
				s_dec_op.u4_size = sizeof(ivd_video_decode_op_t);
				s_dec_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
				s_dec_ip.pv_stream_buffer = NULL;
				s_dec_ip.u4_num_Bytes = 0;
				s_dec_ip.s_out_buffer.u4_min_out_buf_size[0] = 0;
				s_dec_ip.s_out_buffer.u4_min_out_buf_size[1] = 0;
				s_dec_ip.s_out_buffer.u4_num_bufs = 0;
				status = ih264d_api_function(m_codecCtx, &s_dec_ip, &s_dec_op);
				if (status != 0)
					break;
				cemu_assert_debug(s_dec_op.u4_output_present != 0); // should never be zero?
				if(s_dec_op.u4_output_present == 0)
					continue;
				if (H264_IsBotW())
				{
					if (s_dec_op.s_disp_frm_buf.u4_y_wd == 1920 && s_dec_op.s_disp_frm_buf.u4_y_ht == 1088)
						s_dec_op.s_disp_frm_buf.u4_y_ht = 1080;
				}
				DecodeResult tmpResult;
				tmpResult.frameReady = s_dec_op.u4_output_present != 0;
				tmpResult.timestamp = m_timestamps[s_dec_op.u4_ts];
				tmpResult.imageOutput = m_imageBuffers[s_dec_op.u4_ts];
				tmpResult.decodeOutput = s_dec_op;
				AddBufferedResult(tmpResult);
				CopyImageToResultBuffer((uint8*)s_dec_op.s_disp_frm_buf.pv_y_buf, (uint8*)s_dec_op.s_disp_frm_buf.pv_u_buf, (uint8*)m_imageBuffers[s_dec_op.u4_ts], s_dec_op);
			}
			results = std::move(m_bufferedResults);
			return results;
		}

		void CopyImageToResultBuffer(uint8* yIn, uint8* uvIn, uint8* bufOut, ivd_video_decode_op_t& decodeInfo)
		{
			uint32 imageWidth = decodeInfo.s_disp_frm_buf.u4_y_wd;
			uint32 imageHeight = decodeInfo.s_disp_frm_buf.u4_y_ht;

			size_t inputStride = decodeInfo.s_disp_frm_buf.u4_y_strd;
			size_t outputStride = (imageWidth + 0xFF) & ~0xFF;

			// copy Y
			uint8* yOut = bufOut;
			for (uint32 row = 0; row < imageHeight; row++)
			{
				memcpy(yOut, yIn, imageWidth);
				yIn += inputStride;
				yOut += outputStride;
			}

			// copy UV
			uint8* uvOut = bufOut + outputStride * imageHeight;
			for (uint32 row = 0; row < imageHeight/2; row++)
			{
				memcpy(uvOut, uvIn, imageWidth);
				uvIn += inputStride;
				uvOut += outputStride;
			}
		}

	private:

		bool DetermineBufferSizes(void* data, uint32 length, uint32& numByteConsumed)
		{
			numByteConsumed = 0;
			UpdateParameters(true);

			ivd_video_decode_ip_t s_dec_ip{ 0 };
			ivd_video_decode_op_t s_dec_op{ 0 };
			s_dec_ip.u4_size = sizeof(ivd_video_decode_ip_t);
			s_dec_op.u4_size = sizeof(ivd_video_decode_op_t);

			s_dec_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
			s_dec_ip.pv_stream_buffer =	(uint8*)data;
			s_dec_ip.u4_num_Bytes = length;
			s_dec_ip.s_out_buffer.u4_num_bufs = 0;
			WORD32 status = ih264d_api_function(m_codecCtx, &s_dec_ip, &s_dec_op);
			if (status != 0)
			{
				cemuLog_log(LogType::Force, "H264: Unable to determine buffer sizes for stream");
				return false;
			}
			numByteConsumed = s_dec_op.u4_num_bytes_consumed;
			cemu_assert(status == 0);
			if (s_dec_op.u4_pic_wd == 0 || s_dec_op.u4_pic_ht == 0)
				return false;
			UpdateParameters(false);	
			ReinitBuffers();
			return true;
		}

		void ReinitBuffers()
		{
			ivd_ctl_getbufinfo_ip_t s_ctl_ip{ 0 };
			ivd_ctl_getbufinfo_op_t s_ctl_op{ 0 };
			WORD32 outlen = 0;

			s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
			s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_GETBUFINFO;
			s_ctl_ip.u4_size = sizeof(ivd_ctl_getbufinfo_ip_t);
			s_ctl_op.u4_size = sizeof(ivd_ctl_getbufinfo_op_t);

			WORD32 status = ih264d_api_function(m_codecCtx, &s_ctl_ip, &s_ctl_op);
			cemu_assert(!status);

			// allocate
			for (uint32 i = 0; i < s_ctl_op.u4_num_disp_bufs; i++)
			{
				m_displayBuf.emplace_back().resize(s_ctl_op.u4_min_out_buf_size[0] + s_ctl_op.u4_min_out_buf_size[1]);
			}
			// set
			ivd_set_display_frame_ip_t s_set_display_frame_ip{ 0 }; // make sure to zero-initialize this. The codec seems to check the first 3 pointers/sizes per frame, regardless of the value of u4_num_bufs
			ivd_set_display_frame_op_t s_set_display_frame_op{ 0 };

			s_set_display_frame_ip.e_cmd = IVD_CMD_SET_DISPLAY_FRAME;
			s_set_display_frame_ip.u4_size = sizeof(ivd_set_display_frame_ip_t);
			s_set_display_frame_op.u4_size = sizeof(ivd_set_display_frame_op_t);

			cemu_assert_debug(s_ctl_op.u4_min_num_out_bufs == 2);
			cemu_assert_debug(s_ctl_op.u4_min_out_buf_size[0] != 0 && s_ctl_op.u4_min_out_buf_size[1] != 0);

			s_set_display_frame_ip.num_disp_bufs = s_ctl_op.u4_num_disp_bufs;

			for (uint32 i = 0; i < s_ctl_op.u4_num_disp_bufs; i++)
			{
				s_set_display_frame_ip.s_disp_buffer[i].u4_num_bufs = 2;
				s_set_display_frame_ip.s_disp_buffer[i].u4_min_out_buf_size[0] = s_ctl_op.u4_min_out_buf_size[0];
				s_set_display_frame_ip.s_disp_buffer[i].u4_min_out_buf_size[1] = s_ctl_op.u4_min_out_buf_size[1];
				s_set_display_frame_ip.s_disp_buffer[i].pu1_bufs[0] = m_displayBuf[i].data() + 0;
				s_set_display_frame_ip.s_disp_buffer[i].pu1_bufs[1] = m_displayBuf[i].data() + s_ctl_op.u4_min_out_buf_size[0];
			}

			status = ih264d_api_function(m_codecCtx, &s_set_display_frame_ip, &s_set_display_frame_op);
			cemu_assert(!status);


			// mark all as released (available)
			for (uint32 i = 0; i < s_ctl_op.u4_num_disp_bufs; i++)
			{
				ivd_rel_display_frame_ip_t s_video_rel_disp_ip{ 0 };
				ivd_rel_display_frame_op_t s_video_rel_disp_op{ 0 };

				s_video_rel_disp_ip.e_cmd = IVD_CMD_REL_DISPLAY_FRAME;
				s_video_rel_disp_ip.u4_size = sizeof(ivd_rel_display_frame_ip_t);
				s_video_rel_disp_op.u4_size = sizeof(ivd_rel_display_frame_op_t);
				s_video_rel_disp_ip.u4_disp_buf_id = i;

				status = ih264d_api_function(m_codecCtx, &s_video_rel_disp_ip, &s_video_rel_disp_op);
				cemu_assert(!status);
			}
		}

		void ResetDecoder()
		{
			ivd_ctl_reset_ip_t s_ctl_ip;
			ivd_ctl_reset_op_t s_ctl_op;

			s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
			s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_RESET;
			s_ctl_ip.u4_size = sizeof(ivd_ctl_reset_ip_t);
			s_ctl_op.u4_size = sizeof(ivd_ctl_reset_op_t);

			WORD32 status = ih264d_api_function(m_codecCtx, (void*)&s_ctl_ip, (void*)&s_ctl_op);
			cemu_assert_debug(status == 0);
		}

		void UpdateParameters(bool headerDecodeOnly)
		{
			ih264d_ctl_set_config_ip_t s_h264d_ctl_ip{ 0 };
			ih264d_ctl_set_config_op_t s_h264d_ctl_op{ 0 };
			ivd_ctl_set_config_ip_t* ps_ctl_ip = &s_h264d_ctl_ip.s_ivd_ctl_set_config_ip_t;
			ivd_ctl_set_config_op_t* ps_ctl_op = &s_h264d_ctl_op.s_ivd_ctl_set_config_op_t;

			ps_ctl_ip->u4_disp_wd = 0;
			ps_ctl_ip->e_frm_skip_mode = IVD_SKIP_NONE;
			ps_ctl_ip->e_frm_out_mode = m_isBufferedMode ? IVD_DISPLAY_FRAME_OUT : IVD_DECODE_FRAME_OUT;
			ps_ctl_ip->e_vid_dec_mode = headerDecodeOnly ? IVD_DECODE_HEADER : IVD_DECODE_FRAME;
			ps_ctl_ip->e_cmd = IVD_CMD_VIDEO_CTL;
			ps_ctl_ip->e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
			ps_ctl_ip->u4_size = sizeof(ih264d_ctl_set_config_ip_t);
			ps_ctl_op->u4_size = sizeof(ih264d_ctl_set_config_op_t);

			WORD32 status = ih264d_api_function(m_codecCtx, &s_h264d_ctl_ip, &s_h264d_ctl_op);
			cemu_assert(status == 0);
		}

		/* In non-flush mode we have a delay of (at least?) 5 frames */
		void AddBufferedResult(DecodeResult& decodeResult)
		{
			if (decodeResult.frameReady)
				m_bufferedResults.emplace_back(decodeResult);
		}

		void GetCurrentBufferedResult(DecodeResult& decodeResult)
		{
			cemu_assert(!m_bufferedResults.empty());
			if (m_bufferedResults.empty())
			{
				decodeResult.frameReady = false;
				return;
			}
			decodeResult = m_bufferedResults.front();
			m_bufferedResults.erase(m_bufferedResults.begin());
		}
	private:
		iv_obj_t* m_codecCtx{nullptr};
		bool m_hasBufferSizeInfo{ false };
		bool m_isBufferedMode{ false };
		double m_timestamps[64];
		void* m_imageBuffers[64];
		uint32 m_timestampIndex{0};
		std::vector<DecodeResult> m_bufferedResults;
		uint32 m_numDecodedFrames{0};
		std::vector<std::vector<uint8>> m_displayBuf;
	};

	H264DEC_STATUS H264DECGetImageSize(uint8* stream, uint32 length, uint32 offset, uint32be* outputWidth, uint32be* outputHeight)
	{
		cemu_assert(offset <= length);

		uint32 imageWidth, imageHeight;
		
		if (H264AVCDecoder::GetImageInfo(stream, length, imageWidth, imageHeight))
		{
			if (H264_IsBotW())
			{
				if (imageWidth == 1920 && imageHeight == 1088)
					imageHeight = 1080;
			}
			*outputWidth = imageWidth;
			*outputHeight = imageHeight;
		}
		else
		{
			*outputWidth = 0;
			*outputHeight = 0;
			return H264DEC_STATUS::BAD_STREAM;
		}

		return H264DEC_STATUS::SUCCESS;
	}

	uint32 H264DECInitParam(uint32 workMemorySize, void* workMemory)
	{
		H264Context* ctx = (H264Context*)workMemory;
		*ctx = {};
		return 0;
	}

	std::unordered_map<uint32, H264AVCDecoder*> sDecoderSessions;
	std::mutex sDecoderSessionsMutex;
	std::atomic_uint32_t sCurrentSessionHandle{ 1 };

	static H264AVCDecoder* _CreateDecoderSession(uint32& handleOut)
	{
		std::unique_lock _lock(sDecoderSessionsMutex);
		handleOut = sCurrentSessionHandle.fetch_add(1);
		H264AVCDecoder* session = new H264AVCDecoder();
		sDecoderSessions.try_emplace(handleOut, session);
		return session;
	}

	static H264AVCDecoder* _AcquireDecoderSession(uint32 handle)
	{
		std::unique_lock _lock(sDecoderSessionsMutex);
		auto it = sDecoderSessions.find(handle);
		if (it == sDecoderSessions.end())
			return nullptr;
		H264AVCDecoder* session = it->second;
		if (sDecoderSessions.size() >= 5)
		{
			cemuLog_log(LogType::Force, "H264: Warning - more than 5 active sessions");
			cemu_assert_suspicious();
		}
		return session;
	}

	static void _ReleaseDecoderSession(H264AVCDecoder* session)
	{
		std::unique_lock _lock(sDecoderSessionsMutex);

	}

	static void _DestroyDecoderSession(uint32 handle)
	{
		std::unique_lock _lock(sDecoderSessionsMutex);
		auto it = sDecoderSessions.find(handle);
		if (it == sDecoderSessions.end())
			return;
		H264AVCDecoder* session = it->second;
		session->Destroy();
		delete session;
		sDecoderSessions.erase(it);
	}

	uint32 H264DECOpen(void* workMemory)
	{
		H264Context* ctx = (H264Context*)workMemory;
		uint32 sessionHandle;
		_CreateDecoderSession(sessionHandle);
		ctx->sessionHandle = sessionHandle;
		return 0;
	}

	uint32 H264DECClose(void* workMemory)
	{
		if (workMemory)
		{
			H264Context* ctx = (H264Context*)workMemory;
			_DestroyDecoderSession(ctx->sessionHandle);
		}
		return 0;
	}

	uint32 H264DECBegin(void* workMemory)
	{
		H264Context* ctx = (H264Context*)workMemory;
		H264AVCDecoder* session = _AcquireDecoderSession(ctx->sessionHandle);
		if (!session)
		{
			cemuLog_log(LogType::Force, "H264DECBegin(): Invalid session");
			return 0;
		}
		session->Init(ctx->Param.outputPerFrame == 0);
		_ReleaseDecoderSession(session);
		return 0;
	}

	void H264DoFrameOutputCallback(H264Context* ctx, H264AVCDecoder::DecodeResult& decodeResult);

	void _async_H264DECEnd(coreinit::OSEvent* executeDoneEvent, H264AVCDecoder* session, H264Context* ctx, std::vector<H264AVCDecoder::DecodeResult>* decodeResultsOut)
	{
		*decodeResultsOut = session->Flush();
		coreinit::OSSignalEvent(executeDoneEvent);
	}

	H264DEC_STATUS H264DECEnd(void* workMemory)
	{
		H264Context* ctx = (H264Context*)workMemory;
		H264AVCDecoder* session = _AcquireDecoderSession(ctx->sessionHandle);
		if (!session)
		{
			cemuLog_log(LogType::Force, "H264DECEnd(): Invalid session");
			return H264DEC_STATUS::SUCCESS;
		}
		StackAllocator<coreinit::OSEvent> executeDoneEvent;
		coreinit::OSInitEvent(executeDoneEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_MANUAL);
		std::vector<H264AVCDecoder::DecodeResult> results;
		auto asyncTask = std::async(std::launch::async, _async_H264DECEnd, executeDoneEvent.GetPointer(), session, ctx, &results);
		coreinit::OSWaitEvent(executeDoneEvent);
		_ReleaseDecoderSession(session);
		if (!results.empty())
		{
			for (auto& itr : results)
				H264DoFrameOutputCallback(ctx, itr);
		}
		return H264DEC_STATUS::SUCCESS;
	}

	H264DEC_STATUS H264DECSetParam_FPTR_OUTPUT(H264Context* ctx, void* outputFunc)
	{
		ctx->Param.outputFunc = outputFunc;
		return H264DEC_STATUS::SUCCESS;
	}

	H264DEC_STATUS H264DECSetParam_OUTPUT_PER_FRAME(H264Context* ctx, uint32 outputPerFrame)
	{
		ctx->Param.outputPerFrame = outputPerFrame != 0 ? 1 : 0;
		return H264DEC_STATUS::SUCCESS;
	}

	H264DEC_STATUS H264DECSetParam_USER_MEMORY(H264Context* ctx, MEMPTR<void*>* userMemoryParamPtr)
	{
		ctx->Param.userMemoryParam = *userMemoryParamPtr;
		return H264DEC_STATUS::SUCCESS;
	}

	H264DEC_STATUS H264DECSetParam(H264Context* ctx, uint32 paramId, void* paramValue)
	{
		const uint32 PARAMID_FPTR_OUTPUT = 0x1;
		const uint32 PARAMID_OUTPUT_PER_FRAME = 0x20000002;
		const uint32 PARAMID_USER_MEMORY = 0x70000001;
		const uint32 PARAMID_UKN = 0x20000030;

		if (paramId == PARAMID_FPTR_OUTPUT)
		{
			ctx->Param.outputFunc = paramValue;
		}
		else if (paramId == PARAMID_USER_MEMORY)
		{
			ctx->Param.userMemoryParam = paramValue;
		}
		else if (paramId == PARAMID_OUTPUT_PER_FRAME)
		{
			ctx->Param.outputPerFrame = *(uint8be*)paramValue != 0;
		}
		else if (paramId == PARAMID_UKN)
		{
			// unknown purpose, seen in MK8. paramValue points to a bool
		}
		else
		{
			cemuLog_log(LogType::Force, "h264Export_H264DECSetParam(): Unsupported parameterId 0x{:08x}\n", paramId);
			cemu_assert_unimplemented();
		}
		return H264DEC_STATUS::SUCCESS;
	}

	uint32 H264DECSetBitstream(void* workMemory, void* ptr, uint32 length, double timestamp)
	{
		H264Context* ctx = (H264Context*)workMemory;
		ctx->BitStream.ptr = ptr;
		ctx->BitStream.length = length;
		ctx->BitStream.timestamp = timestamp;
		return 0;
	}


	struct H264DECFrameOutput
	{
		/* +0x00 */ uint32be result;
		/* +0x04 */ uint32be padding04;
		/* +0x08 */ betype<double> timestamp;
		/* +0x10 */ uint32be frameWidth;
		/* +0x14 */ uint32be frameHeight;
		/* +0x18 */ uint32be bytesPerRow;
		/* +0x1C */ uint32be cropEnable;
		/* +0x20 */ uint32be cropTop;
		/* +0x24 */ uint32be cropBottom;
		/* +0x28 */ uint32be cropLeft;
		/* +0x2C */ uint32be cropRight;

		/* +0x30 */ uint32be ukn30;
		/* +0x34 */ uint32be ukn34;
		/* +0x38 */ uint32be ukn38;
		/* +0x3C */ uint32be ukn3C;
		/* +0x40 */ uint32be ukn40;

		/* +0x44 */ MEMPTR<uint8> imagePtr;

		/* +0x48 */ uint32 vuiEnable;
		/* +0x4C */ MPTR   vuiPtr;
		/* +0x50 */ sint32 unused[10];
	};

	struct H264OutputCBStruct
	{
		uint32be frameCount;
		MEMPTR<MEMPTR<H264DECFrameOutput>> resultArray;
		uint32be userParam;
	};

	static_assert(sizeof(H264OutputCBStruct) == 12);

	void H264DoFrameOutputCallback(H264Context* ctx, H264AVCDecoder::DecodeResult& decodeResult)
	{
		sint32 outputFrameCount = 1;

		cemu_assert(outputFrameCount < 8);

		StackAllocator<MEMPTR<void>, 8> stack_resultPtrArray;
		StackAllocator<H264DECFrameOutput, 8> stack_decodedFrameResult;

		for (sint32 i = 0; i < outputFrameCount; i++)
			stack_resultPtrArray[i] = stack_decodedFrameResult + i;

		H264DECFrameOutput* frameOutput = stack_decodedFrameResult + 0;
		memset(frameOutput, 0x00, sizeof(H264DECFrameOutput));
		frameOutput->imagePtr = (uint8*)decodeResult.imageOutput;
		frameOutput->result = 100;
		frameOutput->timestamp = decodeResult.timestamp;
		frameOutput->frameWidth = decodeResult.decodeOutput.u4_pic_wd;
		frameOutput->frameHeight = decodeResult.decodeOutput.u4_pic_ht;
		frameOutput->bytesPerRow = (decodeResult.decodeOutput.u4_pic_wd + 0xFF) & ~0xFF;
		frameOutput->cropEnable = decodeResult.decodeOutput.u1_frame_cropping_flag;
		frameOutput->cropTop = decodeResult.decodeOutput.u1_frame_cropping_rect_top_ofst;
		frameOutput->cropBottom = decodeResult.decodeOutput.u1_frame_cropping_rect_bottom_ofst;
		frameOutput->cropLeft = decodeResult.decodeOutput.u1_frame_cropping_rect_left_ofst;
		frameOutput->cropRight = decodeResult.decodeOutput.u1_frame_cropping_rect_right_ofst;

		StackAllocator<H264OutputCBStruct> stack_fptrOutputData;
		stack_fptrOutputData->frameCount = outputFrameCount;
		stack_fptrOutputData->resultArray = (MEMPTR<H264DECFrameOutput>*)stack_resultPtrArray.GetPointer();
		stack_fptrOutputData->userParam = ctx->Param.userMemoryParam.GetBEValue();

		// FPTR callback
		if (!ctx->Param.outputFunc.IsNull())
		{
			cemuLog_log(LogType::H264, "H264: Outputting frame via callback. Timestamp: {} Buffer 0x{:08x} UserParam 0x{:08x}", (double)decodeResult.timestamp, (uint32)frameOutput->imagePtr.GetMPTR(), ctx->Param.userMemoryParam.GetMPTR());
			PPCCoreCallback(ctx->Param.outputFunc.GetMPTR(), stack_fptrOutputData.GetMPTR());
		}
	}

	void _async_H264DECExecute(coreinit::OSEvent* executeDoneEvent, H264AVCDecoder* session, H264Context* ctx, void* imageOutput, H264AVCDecoder::DecodeResult* decodeResult)
	{
		session->Decode(ctx->BitStream.ptr.GetPtr(), ctx->BitStream.length, ctx->BitStream.timestamp, imageOutput, *decodeResult);
		coreinit::OSSignalEvent(executeDoneEvent);
	}

	uint32 H264DECExecute(void* workMemory, void* imageOutput)
	{
		H264Context* ctx = (H264Context*)workMemory;
		H264AVCDecoder* session = _AcquireDecoderSession(ctx->sessionHandle);
		if (!session)
		{
			cemuLog_log(LogType::Force, "H264DECExecute(): Invalid session");
			return 0;
		}
		StackAllocator<coreinit::OSEvent> executeDoneEvent;
		coreinit::OSInitEvent(executeDoneEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_MANUAL);
		H264AVCDecoder::DecodeResult decodeResult;
		auto asyncTask = std::async(std::launch::async, _async_H264DECExecute, executeDoneEvent.GetPointer(), session, ctx, imageOutput , &decodeResult);
		coreinit::OSWaitEvent(executeDoneEvent);
		_ReleaseDecoderSession(session);
		if(decodeResult.frameReady)
			H264DoFrameOutputCallback(ctx, decodeResult);
		return 0x80 | 100;
	}

	H264DEC_STATUS H264DECCheckDecunitLength(void* workMemory, uint8* data, uint32 maxLength, uint32 offset, uint32be* unitLengthOut)
	{
		// todo: our implementation for this currently doesn't parse slice headers and instead assumes that each frame is encoded into a single NAL slice. For all known cases this is sufficient but it doesn't match console behavior for cases where frames are split into multiple NALs
		if (offset >= maxLength || maxLength < 4)
		{
			return H264DEC_STATUS::INVALID_PARAM;
		}

		data += offset;
		maxLength -= offset;

		NALInputBitstream nalStream(data, maxLength);

		if (nalStream.hasError())
		{
			cemu_assert_debug(false);
			return H264DEC_STATUS::BAD_STREAM;
		}

		// search for start code
		sint32 startCodeOffset = 0;
		bool hasStartcode = false;
		while (startCodeOffset < (sint32)(maxLength - 3))
		{
			if (data[startCodeOffset + 0] == 0x00 && data[startCodeOffset + 1] == 0x00 && data[startCodeOffset + 2] == 0x01)
			{
				hasStartcode = true;
				break;
			}
			startCodeOffset++;
		}
		if (hasStartcode == false)
			return H264DEC_STATUS::BAD_STREAM;
		data += startCodeOffset;
		maxLength -= startCodeOffset;

		// parse NAL data
		while (true)
		{
			if (nalStream.isEndOfStream())
				break;
			RBSPInputBitstream rbspStream;
			if (nalStream.getNextRBSP(rbspStream, true) == false)
				break;

			sint32 streamSubOffset = (sint32)(rbspStream.getBasePtr() - data);
			sint32 streamSubLength = rbspStream.getBaseLength();

			// parse NAL header
			uint8 nalHeaderByte = rbspStream.readU8();
			if ((nalHeaderByte & 0x80) != 0)
			{
				// MSB must be zero
				cemu_assert_debug(false);
				continue;
			}
			uint8 nal_ref_idc = (nalHeaderByte >> 5) & 0x3;
			uint8 nal_unit_type = (nalHeaderByte >> 0) & 0x1f;
			if (nal_unit_type == 14 || nal_unit_type == 20 || nal_unit_type == 21)
			{
				cemu_assert_debug(false);
				continue;
			}
			switch (nal_unit_type)
			{
			case 1:
			case 5:
			{
				*unitLengthOut = (sint32)((rbspStream.getBasePtr() + rbspStream.getBaseLength()) - data) + startCodeOffset;
				return H264DEC_STATUS::SUCCESS;
			}
			case 6:
				// SEI
				break;
			case 7:
				// SPS
				break;
			case 8:
				// PPS
				break;
			case 9:
				// access unit delimiter
				break;
			case 10:
				// end of sequence
				break;
			default:
				cemuLog_logDebug(LogType::Force, "Unsupported NAL unit type {}", nal_unit_type);
				cemu_assert_unimplemented();
				// todo
				break;
			}
		}
		return H264DEC_STATUS::BAD_STREAM;
	}

	void Initialize()
	{
		cafeExportRegister("h264", H264DECCheckMemSegmentation, LogType::H264);
		cafeExportRegister("h264", H264DECMemoryRequirement, LogType::H264);
		cafeExportRegister("h264", H264DECFindDecstartpoint, LogType::H264);
		cafeExportRegister("h264", H264DECFindIdrpoint, LogType::H264);
		cafeExportRegister("h264", H264DECGetImageSize, LogType::H264);

		cafeExportRegister("h264", H264DECInitParam, LogType::H264);
		cafeExportRegister("h264", H264DECOpen, LogType::H264);
		cafeExportRegister("h264", H264DECClose, LogType::H264);
		cafeExportRegister("h264", H264DECBegin, LogType::H264);
		cafeExportRegister("h264", H264DECEnd, LogType::H264);

		cafeExportRegister("h264", H264DECSetParam_FPTR_OUTPUT, LogType::H264);
		cafeExportRegister("h264", H264DECSetParam_OUTPUT_PER_FRAME, LogType::H264);
		cafeExportRegister("h264", H264DECSetParam_USER_MEMORY, LogType::H264);
		cafeExportRegister("h264", H264DECSetParam, LogType::H264);

		cafeExportRegister("h264", H264DECSetBitstream, LogType::H264);
		cafeExportRegister("h264", H264DECExecute, LogType::H264);

		cafeExportRegister("h264", H264DECCheckDecunitLength, LogType::H264);
	}
}
