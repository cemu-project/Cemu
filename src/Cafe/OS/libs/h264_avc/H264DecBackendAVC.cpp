#include "H264DecInternal.h"
#include "util/highresolutiontimer/HighResolutionTimer.h"

extern "C"
{
#include "../dependencies/ih264d/common/ih264_typedefs.h"
#include "../dependencies/ih264d/decoder/ih264d.h"
};

namespace H264
{
	bool H264_IsBotW();

	class H264AVCDecoder : public H264DecoderBackend
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
		}

	  public:
		H264AVCDecoder()
		{
			m_decoderThread = std::thread(&H264AVCDecoder::DecoderThread, this);
		}

		~H264AVCDecoder()
		{
			m_threadShouldExit = true;
			m_decodeSem.increment();
			if (m_decoderThread.joinable())
				m_decoderThread.join();
		}

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

			m_numDecodedFrames = 0;
			m_hasBufferSizeInfo = false;
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

		void PushDecodedFrame(ivd_video_decode_op_t& s_dec_op)
		{
			// copy image data outside of lock since its an expensive operation
			CopyImageToResultBuffer((uint8*)s_dec_op.s_disp_frm_buf.pv_y_buf, (uint8*)s_dec_op.s_disp_frm_buf.pv_u_buf, (uint8*)m_decodedSliceArray[s_dec_op.u4_ts].result.imageOutput, s_dec_op);

			std::unique_lock _l(m_decodeQueueMtx);
			cemu_assert(s_dec_op.u4_ts < m_decodedSliceArray.size());
			auto& result = m_decodedSliceArray[s_dec_op.u4_ts];
			cemu_assert_debug(result.isUsed);
			cemu_assert_debug(s_dec_op.u4_output_present != 0);

			result.result.isDecoded = true;
			result.result.hasFrame = s_dec_op.u4_output_present != 0;
			result.result.frameWidth = s_dec_op.u4_pic_wd;
			result.result.frameHeight = s_dec_op.u4_pic_ht;
			result.result.bytesPerRow = (s_dec_op.u4_pic_wd + 0xFF) & ~0xFF;
			result.result.cropEnable = s_dec_op.u1_frame_cropping_flag;
			result.result.cropTop = s_dec_op.u1_frame_cropping_rect_top_ofst;
			result.result.cropBottom = s_dec_op.u1_frame_cropping_rect_bottom_ofst;
			result.result.cropLeft = s_dec_op.u1_frame_cropping_rect_left_ofst;
			result.result.cropRight = s_dec_op.u1_frame_cropping_rect_right_ofst;

			m_displayQueue.push_back(s_dec_op.u4_ts);

			_l.unlock();
			coreinit::OSSignalEvent(m_displayQueueEvt);
		}

		// called from async worker thread
		void Decode(DecodedSlice& decodedSlice)
		{
			if (!m_hasBufferSizeInfo)
			{
				uint32 numByteConsumed = 0;
				if (!DetermineBufferSizes(decodedSlice.dataToDecode.m_data, decodedSlice.dataToDecode.m_length, numByteConsumed))
				{
					cemuLog_log(LogType::Force, "H264AVC: Unable to determine picture size. Ignoring decode input");
					std::unique_lock _l(m_decodeQueueMtx);
					decodedSlice.result.isDecoded = true;
					decodedSlice.result.hasFrame = false;
					coreinit::OSSignalEvent(m_displayQueueEvt);
					return;
				}
				decodedSlice.dataToDecode.m_length -= numByteConsumed;
				decodedSlice.dataToDecode.m_data = (uint8*)decodedSlice.dataToDecode.m_data + numByteConsumed;
				m_hasBufferSizeInfo = true;
			}

			ivd_video_decode_ip_t s_dec_ip{ 0 };
			ivd_video_decode_op_t s_dec_op{ 0 };
			s_dec_ip.u4_size = sizeof(ivd_video_decode_ip_t);
			s_dec_op.u4_size = sizeof(ivd_video_decode_op_t);

			s_dec_ip.e_cmd = IVD_CMD_VIDEO_DECODE;

			s_dec_ip.u4_ts = std::distance(m_decodedSliceArray.data(), &decodedSlice);
			cemu_assert_debug(s_dec_ip.u4_ts < m_decodedSliceArray.size());

			s_dec_ip.pv_stream_buffer = (uint8*)decodedSlice.dataToDecode.m_data;
			s_dec_ip.u4_num_Bytes = decodedSlice.dataToDecode.m_length;

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
				Decode(decodedSlice);
				return;
			}
			else if (status != 0)
			{
				cemuLog_log(LogType::Force, "H264: Failed to decode frame (error 0x{:08x})", status);
				decodedSlice.result.hasFrame = false;
				cemu_assert_unimplemented();
				return;
			}

			bt.Stop();
			double decodeTime = bt.GetElapsedMilliseconds();

			cemu_assert(s_dec_op.u4_frame_decoded_flag);
			cemu_assert_debug(s_dec_op.u4_num_bytes_consumed == decodedSlice.dataToDecode.m_length);

			cemu_assert_debug(m_isBufferedMode || s_dec_op.u4_output_present); // if buffered mode is disabled, then every input should output a frame (except for partial slices?)

			if (s_dec_op.u4_output_present)
			{
				cemu_assert(s_dec_op.e_output_format == IV_YUV_420SP_UV);
				if (H264_IsBotW())
				{
					if (s_dec_op.s_disp_frm_buf.u4_y_wd == 1920 && s_dec_op.s_disp_frm_buf.u4_y_ht == 1088)
						s_dec_op.s_disp_frm_buf.u4_y_ht = 1080;
				}
				bt.Start();
				PushDecodedFrame(s_dec_op);
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
				cemuLog_log(LogType::H264, "H264Bench | DecodeTime {}ms (no frame output)", decodeTime);
			}

			if (s_dec_op.u4_frame_decoded_flag)
				m_numDecodedFrames++;
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

		void Flush()
		{
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
			// get all frames from the decoder
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
				cemu_assert_debug(s_dec_op.u4_output_present != 0); // should never be false?
				if(s_dec_op.u4_output_present == 0)
					continue;
				if (H264_IsBotW())
				{
					if (s_dec_op.s_disp_frm_buf.u4_y_wd == 1920 && s_dec_op.s_disp_frm_buf.u4_y_ht == 1088)
						s_dec_op.s_disp_frm_buf.u4_y_ht = 1080;
				}
				PushDecodedFrame(s_dec_op);
			}
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

	  private:
		void DecoderThread()
		{
			while(!m_threadShouldExit)
			{
				m_decodeSem.decrementWithWait();
				std::unique_lock _l(m_decodeQueueMtx);
				if (m_decodeQueue.empty())
					continue;
				uint32 decodeIndex = m_decodeQueue.front();
				m_decodeQueue.erase(m_decodeQueue.begin());
				_l.unlock();
				if(decodeIndex == CMD_FLUSH)
				{
					Flush();
					_l.lock();
					cemu_assert_debug(m_decodeQueue.empty()); // after flushing the queue should be empty since the sender is waiting for the flush to complete
					_l.unlock();
					coreinit::OSSignalEvent(m_flushEvt);
				}
				else
				{
					auto& decodedSlice = m_decodedSliceArray[decodeIndex];
					Decode(decodedSlice);
				}
			}
		}

		iv_obj_t* m_codecCtx{nullptr};
		bool m_hasBufferSizeInfo{ false };
		bool m_isBufferedMode{ false };
		uint32 m_numDecodedFrames{0};
		std::vector<std::vector<uint8>> m_displayBuf;

		std::thread m_decoderThread;
		std::atomic_bool m_threadShouldExit{false};
	};

	H264DecoderBackend* CreateAVCDecoder()
	{
		return new H264AVCDecoder();
	}
};
