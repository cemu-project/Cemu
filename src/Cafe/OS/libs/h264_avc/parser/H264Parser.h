#pragma once

/* stream parsers */

class RBSPInputBitstream
{
public:
	RBSPInputBitstream() {};

	RBSPInputBitstream(uint8* stream, uint32 length)
	{
		this->streamPtr = stream;
		this->streamLength = length;
		if (streamLength >= 1)
			currentRBSPByte = streamPtr[0];
		else
			currentRBSPByte = 0;
		errorFlag = false;
	}

	bool hasError() const
	{
		return errorFlag;
	}

	bool isByteAligned() const
	{
		return bitIndex == 0;
	}

	uint8 readU8()
	{
		if (readIndex >= streamLength)
			return 0;
		cemu_assert_debug(bitIndex == 0);
		uint8 v = _getCurrentRBSPByte();
		_nextRBSPByte();
		return v;
	}

	uint8 readBit()
	{
		if (readIndex >= streamLength)
			return 0;
		uint8 v = _getCurrentRBSPByte();
		uint8 bitValue = (v >> (7 - bitIndex)) & 1;
		bitIndex += 1;
		if (bitIndex >= 8)
		{
			bitIndex = 0;
			_nextRBSPByte();
		}
		return bitValue;
	}

	template<int N>
	uint32 readBits()
	{
		uint32 v = 0;
		for (sint32 i = 0; i < N; i++)
		{
			v <<= 1;
			v |= (readBit() ? 1 : 0);
		}
		return v;
	}

	uint32 readBits(sint32 N)
	{
		uint32 v = 0;
		for (sint32 i = 0; i < N; i++)
		{
			v <<= 1;
			v |= (readBit() ? 1 : 0);
		}
		return v;
	}

	uint32 readUV_E()
	{
		if (readBit() != 0)
		{
			return 0;
		}
		for (sint32 i = 1; i < 31; i++)
		{
			uint8 bitValue = readBit();
			if (bitValue)
			{
				uint32 rangeBase = (1 << i) - 1;
				// read range bits
				uint32 suffixBits = 0;
				for (sint32 b = 0; b < i; b++)
				{
					suffixBits <<= 1;
					suffixBits |= (readBit() ? 1 : 0);
				}
				return rangeBase + suffixBits;
			}
		}
		cemu_assert_debug(false);
		return 0;
	}

	sint32 readSV_E()
	{
		uint32 t = readUV_E();
		if (t == 0)
			return 0;
		if ((t & 1) != 0)
		{
			// positive
			return (sint32)((t + 1) / 2);
		}
		// negative
		return -(sint32)((t + 1) / 2);
	}

	bool readTrailingRBSPBits()
	{
		// read the stop bit which must always be 1
		if (readBit() == 0)
			return false;
		// read zero bits until byte aligned
		while (bitIndex != 0)
		{
			if (readBit() != 0)
				return false; // invalid padding bit
		}
		// check if we reached end of stream
		if (readIndex >= streamLength)
			return true;
		return false;
	}

	bool more_rbsp_data()
	{
		uint32 storedReadIndex = readIndex;
		uint32 storedBitIndex = bitIndex;
		bool hasMoreRBSPData = readTrailingRBSPBits() == false;
		readIndex = storedReadIndex;
		bitIndex = storedBitIndex;
		currentRBSPByte = streamPtr[readIndex];
		return hasMoreRBSPData;
	}

	bool isEndOfStream()
	{
		return readIndex >= streamLength;
	}


	uint8* getBasePtr()
	{
		return streamPtr;
	}

	sint32 getBaseLength()
	{
		return streamLength;
	}

private:
	uint8 _getCurrentRBSPByte()
	{
		return currentRBSPByte;
	}

	void _nextRBSPByte()
	{
		readIndex += sizeof(uint8);
		if (readIndex >= 2 && streamPtr[readIndex - 2] == 0x00 && streamPtr[readIndex - 1] == 0x00 && streamPtr[readIndex] == 0x03)
		{
			readIndex += 1;
			currentRBSPByte = streamPtr[readIndex];
			return;
		}
		else
		{
			currentRBSPByte = streamPtr[readIndex];
		}
	}

private:
	uint8* streamPtr;
	uint32 streamLength;
	uint32 readIndex{};
	uint8  currentRBSPByte{};
	sint32 bitIndex{};
	bool errorFlag{};
};

class NALInputBitstream
{
public:
	NALInputBitstream(uint8* stream, uint32 length)
	{
		this->nalStreamPtr = stream;
		this->nalStreamLength = length;
		errorFlag = false;
	}

	bool hasError()
	{
		return errorFlag;
	}

	bool getNextRBSP(RBSPInputBitstream& rbspInputBitstream, bool mustEndAtStartCode = false)
	{
		sint32 indexNextStartSignature = findNextStartSignature();

		if (indexNextStartSignature < 0)
		{
			if (mustEndAtStartCode)
				return false;
			indexNextStartSignature = nalStreamLength;
		}
		if (indexNextStartSignature <= readIndex)
			return false;
		// skip current start signature
		if ((nalStreamLength - readIndex) >= 3 && nalStreamPtr[readIndex + 0] == 0 && nalStreamPtr[readIndex + 1] == 0 && nalStreamPtr[readIndex + 2] == 1)
		{
			readIndex += 3;
		}
		else if ((nalStreamLength - readIndex) >= 3 && nalStreamPtr[readIndex + 0] == 0 && nalStreamPtr[readIndex + 1] == 0 && nalStreamPtr[readIndex + 2] == 0 && nalStreamPtr[readIndex + 3] == 1)
		{
			readIndex += 4;
		}
		else
		{
			// no start signature
		}
		cemu_assert(readIndex <= indexNextStartSignature);
		cemu_assert_debug(readIndex != indexNextStartSignature);
		// create rbsp substream
		rbspInputBitstream = RBSPInputBitstream(nalStreamPtr + readIndex, indexNextStartSignature - readIndex);
		readIndex = indexNextStartSignature;
		return true;
	}

	bool isEndOfStream()
	{
		return readIndex >= nalStreamLength;
	}

private:
	sint32 findNextStartSignature()
	{
		if (readIndex >= nalStreamLength)
			return -1;
		sint32 offset = readIndex;
		// if there is a start signature at the current address, skip it
		if ((offset + 3) <= nalStreamLength && nalStreamPtr[offset + 0] == 0x00 && nalStreamPtr[offset + 1] == 0x00 && nalStreamPtr[offset + 2] == 0x01)
		{
			offset += 3;
		}
		else if ((offset + 4) <= nalStreamLength && nalStreamPtr[offset + 0] == 0x00 && nalStreamPtr[offset + 1] == 0x00 && nalStreamPtr[offset + 2] == 0x00 && nalStreamPtr[offset + 3] == 0x01)
		{
			offset += 4;
		}
		while (offset < (nalStreamLength - 3))
		{
			if (nalStreamPtr[offset + 0] == 0x00 && nalStreamPtr[offset + 1] == 0x00)
			{
				if (nalStreamPtr[offset + 2] == 0x01)
				{
					return offset;
				}
				else if ((nalStreamLength - offset) >= 4 && nalStreamPtr[offset + 2] == 0x00 && nalStreamPtr[offset + 3] == 0x01)
				{
					return offset;
				}
			}
			offset++;
		}
		return -1;
	}

private:
	uint8* nalStreamPtr;
	sint32 nalStreamLength;
	sint32 readIndex{};
	bool errorFlag{};
};

/* H264 NAL parser */

struct h264_scaling_matrix4x4_t
{
	uint8 isPresent;
	uint8 UseDefaultScalingMatrix;
	sint32 list[4*4];
};

struct h264_scaling_matrix8x8_t
{
	uint8 isPresent;
	uint8 UseDefaultScalingMatrix;
	sint32 list[8*8];
};

struct h264State_pic_parameter_set_t
{
	uint32 pic_parameter_set_id;
	uint32 seq_parameter_set_id;
	uint8 entropy_coding_mode_flag;
	uint8 bottom_field_pic_order_in_frame_present_flag; // alias pic_order_present_flag
	uint32 num_slice_groups_minus1;
	// slice group
	uint32 slice_group_map_type;
	// todo - some variable sized fields

	uint32 slice_group_change_rate_minus1; // todo

	uint32 num_ref_idx_l0_default_active_minus1;
	uint32 num_ref_idx_l1_default_active_minus1;
	uint8 weighted_pred_flag;
	uint8 weighted_bipred_idc;
	sint32 pic_init_qp_minus26;
	sint32 pic_init_qs_minus26;
	sint32 chroma_qp_index_offset;

	uint8 deblocking_filter_control_present_flag;
	uint8 constrained_intra_pred_flag;
	uint8 redundant_pic_cnt_present_flag;

	uint8 transform_8x8_mode_flag;
	uint8 pic_scaling_matrix_present_flag;
	h264_scaling_matrix4x4_t ScalingList4x4[6];
	h264_scaling_matrix8x8_t ScalingList8x8[6];

	sint32 second_chroma_qp_index_offset;
};


struct h264State_seq_parameter_set_t
{
	uint8 profile_idc; // 0x64 = high profile
	uint8 constraint; // 6 flags + 2 reserved bits
	uint8 level_idc; // 0x29 = level 4.1

	uint32 seq_parameter_set_id;
	uint32 chroma_format_idc;

	uint8 separate_colour_plane_flag; // aka residual_colour_transform_flag

	uint32 bit_depth_luma_minus8;
	uint32 bit_depth_chroma_minus8;
	uint8 qpprime_y_zero_transform_bypass_flag;
	uint8 seq_scaling_matrix_present_flag;
	h264_scaling_matrix4x4_t ScalingList4x4[6];
	h264_scaling_matrix8x8_t ScalingList8x8[6];

	uint32 log2_max_frame_num_minus4;
	uint32 pic_order_cnt_type;

	// picture order count type 0
	uint32 log2_max_pic_order_cnt_lsb_minus4;

	uint32 num_ref_frames;
	uint32 gaps_in_frame_num_value_allowed_flag;
	uint32 pic_width_in_mbs_minus1;
	uint32 pic_height_in_map_units_minus1;
	uint8 frame_mbs_only_flag;
	uint8 mb_adaptive_frame_field_flag;

	uint8 direct_8x8_inference_flag;
	
	uint8 frame_cropping_flag;
	uint32 frame_crop_left_offset;
	uint32 frame_crop_right_offset;
	uint32 frame_crop_top_offset;
	uint32 frame_crop_bottom_offset;

	uint32 getMaxFrameNum() const
	{
		return 1<<(log2_max_frame_num_minus4+4);
	}

};

struct H264SliceType
{
	H264SliceType() {};
	H264SliceType(uint32 slice_type) : m_sliceType(slice_type) {};

	bool isSliceTypeP() const { return (this->m_sliceType % 5) == 0; };
	bool isSliceTypeB() const { return (this->m_sliceType % 5) == 1; };
	bool isSliceTypeI() const { return (this->m_sliceType % 5) == 2; };
	bool isSliceTypeSP() const { return (this->m_sliceType % 5) == 3; };
	bool isSliceTypeSI() const { return (this->m_sliceType % 5) == 4; };

private:
	uint32 m_sliceType{};
};

typedef struct
{
	uint8 nal_ref_idc;
	uint8 nal_unit_type;

	uint32 first_mb_in_slice;
	H264SliceType slice_type;
	uint32 pic_parameter_set_id;

	uint32 frame_num;

	uint8 field_pic_flag;
	uint8 bottom_field_flag;

	uint32 idr_pic_id;
	
	uint32 pic_order_cnt_lsb;
	sint32 delta_pic_order_cnt_bottom;

	uint32 redundant_pic_cnt;

	// slice type B
	uint8 direct_spatial_mv_pred_flag;

	// slice type P, SP, B
	uint8 num_ref_idx_active_override_flag;
	uint32 num_ref_idx_l0_active_minus1;
	uint32 num_ref_idx_l1_active_minus1;

	// ref_pic_list_modification_flag_l0
	struct  
	{
		uint8 type;
		uint32 abs_diff_pic_num_minus1;
		uint32 long_term_pic_num;
	}pic_list_modification0Array[32];
	sint32 pic_list_modification0Count;

	// ref_pic_list_modification_flag_l1
	struct  
	{
		uint8 type;
		uint32 abs_diff_pic_num_minus1;
		uint32 long_term_pic_num;
	}pic_list_modification1Array[32];
	sint32 pic_list_modification1Count;

	// memory_management_control_operation
	enum
	{
		MEMOP_END = 0,
		MEMOP_REMOVE_REF_FROM_SHORT_TERM = 1,
		MEMOP_REMOVE_REF_FROM_LONG_TERM = 2,
		MEMOP_MAKE_LONG_TERM_REF = 3,
		MEMOP_MAX_LONG_TERM_INDEX = 4,
		MEMOP_REMOVE_ALL_REFS = 5,
		MEMOP_MAKE_CURRENT_LONG_TERM_REF = 6
	};

	uint8 adaptive_ref_pic_marking_mode_flag;
	struct  
	{
		uint8 op;
		uint32 difference_of_pic_nums_minus1;
		uint32 long_term_pic_num;
		uint32 long_term_frame_idx;
		uint32 max_long_term_frame_idx_plus1;
	}memory_management_control_operation[16];
	sint32 memory_management_control_operation_num;

	// derived values
	uint8 IdrPicFlag;

	struct  
	{
		uint32 TopFieldOrderCnt;
	}calculated;
}nal_slice_header_t;

typedef struct
{
	sint32 streamSubOffset;
	sint32 streamSubSize;
	nal_slice_header_t header;
}nal_slice_info_t;

typedef struct  
{
	bool hasSPS;
	bool hasPPS;
	// list of NAL slices
	sint32 sliceCount;
	std::array<nal_slice_info_t, 32> sliceInfo;
}h264ParserOutput_t;

typedef struct
{
	nal_slice_header_t slice_header;
}nal_slice_t;

typedef struct  
{
	//static const int MAX_SLICES = 32;
	bool hasSPS;
	bool hasPPS;
	h264State_seq_parameter_set_t sps;
	h264State_pic_parameter_set_t pps;
	struct  
	{
		uint32 prevPicOrderCntMsb;
		uint32 prevPicOrderCntLsb;
		uint32 prevFrameNum;
		uint32 prevFrameNumOffset;
	}picture_order;
}h264ParserState_t;

void h264Parse(h264ParserState_t* h264ParserState, h264ParserOutput_t* output, uint8* data, uint32 length, bool parseSlices = true);
sint32 h264GetUnitLength(h264ParserState_t* h264ParserState, uint8* data, uint32 length);

bool h264Parser_ParseSPS(uint8* data, uint32 length, h264State_seq_parameter_set_t& sps);

void h264Parser_getScalingMatrix4x4(h264State_seq_parameter_set_t* sps, h264State_pic_parameter_set_t* pps, nal_slice_header_t* sliceHeader, sint32 index, uint8* matrix4x4);
void h264Parser_getScalingMatrix8x8(h264State_seq_parameter_set_t* sps, h264State_pic_parameter_set_t* pps, nal_slice_header_t* sliceHeader, sint32 index, uint8* matrix8x8);

static sint32 h264GetFramePitch(sint32 width)
{
	return (width+0xFF)&~0xFF;
}

static sint32 h264GetFrameSize(sint32 width, sint32 height)
{
	sint32 pitch = h264GetFramePitch(width);
	return height * pitch + (height / 2) * pitch;
}