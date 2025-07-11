#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Common/cpu_features.h"

#if defined(ARCH_X86_64) && defined(__GNUC__)
#include <immintrin.h>
#elif defined(__aarch64__)
#include <arm_neon.h>
#endif

struct  
{
	struct CacheEntry
	{
		// input data
		const void* lastPtr;
		uint32 lastCount;
		LattePrimitiveMode lastPrimitiveMode;
		LatteIndexType lastIndexType;
		uint64 lastUsed;
		// output
		uint32 indexMin;
		uint32 indexMax;
		Renderer::INDEX_TYPE renderIndexType;
		uint32 outputCount;
		Renderer::IndexAllocation indexAllocation;
	};
	std::array<CacheEntry, 8> entry;
	uint64 currentUsageCounter{0};
}LatteIndexCache{};

void LatteIndices_invalidate(const void* memPtr, uint32 size)
{
	for(auto& entry : LatteIndexCache.entry)
	{
		if (entry.lastPtr >= memPtr && (entry.lastPtr < ((uint8*)memPtr + size)) )
		{
			if(entry.lastPtr != nullptr)
				g_renderer->indexData_releaseIndexMemory(entry.indexAllocation);
			entry.lastPtr = nullptr;
			entry.lastCount = 0;
		}
	}
}

void LatteIndices_invalidateAll()
{
	for(auto& entry : LatteIndexCache.entry)
	{
		if (entry.lastPtr != nullptr)
			g_renderer->indexData_releaseIndexMemory(entry.indexAllocation);
		entry.lastPtr = nullptr;
		entry.lastCount = 0;
	}
}

uint64 LatteIndices_GetNextUsageIndex()
{
	return LatteIndexCache.currentUsageCounter++;
}

uint32 LatteIndices_calculateIndexOutputSize(LattePrimitiveMode primitiveMode, LatteIndexType indexType, uint32 count)
{
	if (primitiveMode == LattePrimitiveMode::QUADS)
	{
		sint32 numQuads = count / 4;
		if (indexType == LatteIndexType::AUTO)
		{
			if(count <= 0xFFFF)
				return numQuads * 6 * sizeof(uint16);
			return numQuads * 6 * sizeof(uint32);
		}
		if (indexType == LatteIndexType::U16_BE || indexType == LatteIndexType::U16_LE)
			return numQuads * 6 * sizeof(uint16);
		if (indexType == LatteIndexType::U32_BE || indexType == LatteIndexType::U32_LE)
			return numQuads * 6 * sizeof(uint32);
		cemu_assert_suspicious();
		return 0;
	}
	else if (primitiveMode == LattePrimitiveMode::QUAD_STRIP)
	{
		if (count <= 3)
		{
			return 0;
		}
		sint32 numQuads = (count-2) / 2;
		if (indexType == LatteIndexType::AUTO)
		{
			if (count <= 0xFFFF)
				return numQuads * 6 * sizeof(uint16);
			return numQuads * 6 * sizeof(uint32);
		}
		if (indexType == LatteIndexType::U16_BE || indexType == LatteIndexType::U16_LE)
			return numQuads * 6 * sizeof(uint16);
		if (indexType == LatteIndexType::U32_BE || indexType == LatteIndexType::U32_LE)
			return numQuads * 6 * sizeof(uint32);
		cemu_assert_suspicious();
		return 0;
	}
	else if (primitiveMode == LattePrimitiveMode::LINE_LOOP)
	{
		count++; // one extra vertex to reconnect the LINE_STRIP to the beginning
		if (indexType == LatteIndexType::AUTO)
		{
			if (count <= 0xFFFF)
				return count * sizeof(uint16);
			return count * sizeof(uint32);
		}
		if (indexType == LatteIndexType::U16_BE || indexType == LatteIndexType::U16_LE)
			return count * sizeof(uint16);
		if (indexType == LatteIndexType::U32_BE || indexType == LatteIndexType::U32_LE)
			return count * sizeof(uint32);
		cemu_assert_suspicious();
		return 0;
	}
	else if(indexType == LatteIndexType::AUTO)
		return 0;
	else if (indexType == LatteIndexType::U16_BE || indexType == LatteIndexType::U16_LE)
		return count * sizeof(uint16);
	else if (indexType == LatteIndexType::U32_BE || indexType == LatteIndexType::U32_LE)
		return count * sizeof(uint32);
	return 0;
}

template<typename T>
void LatteIndices_convertBE(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	const betype<T>* src = (betype<T>*)indexDataInput;
	T* dst = (T*)indexDataOutput;
	for (uint32 i = 0; i < count; i++)
	{
		T v = *src;
		*dst = v;
		indexMin = std::min(indexMin, (uint32)v);
		indexMax = std::max(indexMax, (uint32)v);
		dst++;
		src++;
	}
}

template<typename T>
void LatteIndices_convertLE(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	const T* src = (T*)indexDataInput;
	T* dst = (T*)indexDataOutput;
	for (uint32 i = 0; i < count; i++)
	{
		T v = *src;
		*dst = v;
		indexMin = std::min(indexMin, (uint32)v);
		indexMax = std::max(indexMax, (uint32)v);
		dst++;
		src++;
	}
}

template<typename T>
void LatteIndices_unpackQuadsAndConvert(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	sint32 numQuads = count / 4;
	const betype<T>* src = (betype<T>*)indexDataInput;
	T* dst = (T*)indexDataOutput;
	for (sint32 i = 0; i < numQuads; i++)
	{
		T idx0 = src[0];
		T idx1 = src[1];
		T idx2 = src[2];
		T idx3 = src[3];
		indexMin = std::min(indexMin, (uint32)idx0);
		indexMax = std::max(indexMax, (uint32)idx0);
		indexMin = std::min(indexMin, (uint32)idx1);
		indexMax = std::max(indexMax, (uint32)idx1);
		indexMin = std::min(indexMin, (uint32)idx2);
		indexMax = std::max(indexMax, (uint32)idx2);
		indexMin = std::min(indexMin, (uint32)idx3);
		indexMax = std::max(indexMax, (uint32)idx3);
		dst[0] = idx0;
		dst[1] = idx1;
		dst[2] = idx2;
		dst[3] = idx0;
		dst[4] = idx2;
		dst[5] = idx3;
		src += 4;
		dst += 6;
	}
}

template<typename T>
void LatteIndices_generateAutoQuadIndices(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	sint32 numQuads = count / 4;
	const betype<T>* src = (betype<T>*)indexDataInput;
	T* dst = (T*)indexDataOutput;
	for (sint32 i = 0; i < numQuads; i++)
	{
		T idx0 = i * 4 + 0;
		T idx1 = i * 4 + 1;
		T idx2 = i * 4 + 2;
		T idx3 = i * 4 + 3;
		dst[0] = idx0;
		dst[1] = idx1;
		dst[2] = idx2;
		dst[3] = idx0;
		dst[4] = idx2;
		dst[5] = idx3;
		src += 4;
		dst += 6;
	}
	indexMin = 0;
	indexMax = std::max(count, 1u) - 1;
}

template<typename T>
void LatteIndices_unpackQuadStripAndConvert(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	if (count <= 3)
		return;
	sint32 numQuads = (count - 2) / 2;
	const betype<T>* src = (betype<T>*)indexDataInput;
	T* dst = (T*)indexDataOutput;
	for (sint32 i = 0; i < numQuads; i++)
	{
		T idx0 = src[0];
		T idx1 = src[1];
		T idx2 = src[2];
		T idx3 = src[3];
		indexMin = std::min(indexMin, (uint32)idx0);
		indexMax = std::max(indexMax, (uint32)idx0);
		indexMin = std::min(indexMin, (uint32)idx1);
		indexMax = std::max(indexMax, (uint32)idx1);
		indexMin = std::min(indexMin, (uint32)idx2);
		indexMax = std::max(indexMax, (uint32)idx2);
		indexMin = std::min(indexMin, (uint32)idx3);
		indexMax = std::max(indexMax, (uint32)idx3);
		dst[0] = idx0;
		dst[1] = idx1;
		dst[2] = idx2;
		dst[3] = idx2;
		dst[4] = idx1;
		dst[5] = idx3;
		src += 2;
		dst += 6;
	}
}

template<typename T>
void LatteIndices_unpackLineLoopAndConvert(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	if (count <= 0)
		return;
	const betype<T>* src = (betype<T>*)indexDataInput;
	T firstIndex = *src;
	T* dst = (T*)indexDataOutput;
	for (sint32 i = 0; i < (sint32)count; i++)
	{
		T idx = *src;
		indexMin = std::min(indexMin, (uint32)idx);
		indexMax = std::max(indexMax, (uint32)idx);
		*dst = idx;
		src++;
		dst++;
	}
	*dst = firstIndex;
}

template<typename T>
void LatteIndices_generateAutoQuadStripIndices(void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	if (count <= 3)
		return;
	sint32 numQuads = (count - 2) / 2;
	T* dst = (T*)indexDataOutput;
	for (sint32 i = 0; i < numQuads; i++)
	{
		T idx0 = i * 2 + 0;
		T idx1 = i * 2 + 1;
		T idx2 = i * 2 + 2;
		T idx3 = i * 2 + 3;
		dst[0] = idx0;
		dst[1] = idx1;
		dst[2] = idx2;
		dst[3] = idx2;
		dst[4] = idx1;
		dst[5] = idx3;
		dst += 6;
	}
	indexMin = 0;
	indexMax = std::max(count, 1u) - 1;
}


template<typename T>
void LatteIndices_generateAutoLineLoopIndices(void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	if (count == 0)
		return;
	T* dst = (T*)indexDataOutput;
	for (sint32 i = 0; i < (sint32)count; i++)
	{
		*dst = (T)i;
		dst++;
	}
	*dst = 0;
	dst++;
	indexMin = 0;
	indexMax = std::max(count, 1u) - 1;
}

#if defined(ARCH_X86_64)
ATTRIBUTE_AVX2
void LatteIndices_fastConvertU16_AVX2(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	// using AVX + AVX2 we can process 16 indices at a time
	const uint16* indicesU16BE = (const uint16*)indexDataInput;
	uint16* indexOutput = (uint16*)indexDataOutput;
	sint32 count16 = count >> 4;
	sint32 countRemaining = count & 15;
	if (count16)
	{
		__m256i mMin = _mm256_set_epi16((sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, 
						           		(sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF, (sint16)0xFFFF);
		__m256i mMax = _mm256_set_epi16(0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000);
		__m256i mShuffle16Swap = _mm256_set_epi8(30, 31, 28, 29, 26, 27, 24, 25, 22, 23, 20, 21, 18, 19, 16, 17, 14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1);

		do
		{
			__m256i mIndexData = _mm256_loadu_si256((const __m256i*)indicesU16BE);
			indicesU16BE += 16;
			_mm_prefetch((const char*)indicesU16BE, _MM_HINT_T0);
			// endian swap
			mIndexData = _mm256_shuffle_epi8(mIndexData, mShuffle16Swap);
			_mm256_store_si256((__m256i*)indexOutput, mIndexData);
			mMin = _mm256_min_epu16(mIndexData, mMin);
			mMax = _mm256_max_epu16(mIndexData, mMax);
			indexOutput += 16;
		} while (--count16);

		// fold 32 to 16 byte
		mMin = _mm256_min_epu16(mMin, _mm256_permute2x128_si256(mMin, mMin, 1));
		mMax = _mm256_max_epu16(mMax, _mm256_permute2x128_si256(mMax, mMax, 1));
		// fold 16 to 8 byte
		mMin = _mm256_min_epu16(mMin, _mm256_shuffle_epi32(mMin, (2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));
		mMax = _mm256_max_epu16(mMax, _mm256_shuffle_epi32(mMax, (2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));

		uint16* mMinU16 = (uint16*)&mMin;
		uint16* mMaxU16 = (uint16*)&mMax;

		indexMin = std::min(indexMin, (uint32)mMinU16[0]);
		indexMin = std::min(indexMin, (uint32)mMinU16[1]);
		indexMin = std::min(indexMin, (uint32)mMinU16[2]);
		indexMin = std::min(indexMin, (uint32)mMinU16[3]);

		indexMax = std::max(indexMax, (uint32)mMaxU16[0]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[1]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[2]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[3]);
	}
	// process remaining indices
	uint32 _minIndex = 0xFFFFFFFF;
	uint32 _maxIndex = 0;
	for (sint32 i = countRemaining; (--i) >= 0;)
	{
		uint16 idx = _swapEndianU16(*indicesU16BE);
		*indexOutput = idx;
		indexOutput++;
		indicesU16BE++;
		_maxIndex = std::max(_maxIndex, (uint32)idx);
		_minIndex = std::min(_minIndex, (uint32)idx);
	}
	// update min/max
	indexMax = std::max(indexMax, _maxIndex);
	indexMin = std::min(indexMin, _minIndex);
}

ATTRIBUTE_SSE41
void LatteIndices_fastConvertU16_SSE41(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	// SSSE3 & SSE4.1 optimized decoding
	const uint16* indicesU16BE = (const uint16*)indexDataInput;
	uint16* indexOutput = (uint16*)indexDataOutput;
	sint32 count8 = count >> 3;
	sint32 countRemaining = count & 7;
	if (count8)
	{
		__m128i mMin = _mm_set_epi16((short)0xFFFF, (short)0xFFFF, (short)0xFFFF, (short)0xFFFF, (short)0xFFFF, (short)0xFFFF, (short)0xFFFF, (short)0xFFFF);
		__m128i mMax = _mm_set_epi16(0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000);
		__m128i mTemp;
		__m128i* mRawIndices = (__m128i*)indicesU16BE;
		indicesU16BE += count8 * 8;
		__m128i* mOutputIndices = (__m128i*)indexOutput;
		indexOutput += count8 * 8;
		__m128i shufmask = _mm_set_epi8(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1);
		while (count8--)
		{
			mTemp = _mm_loadu_si128(mRawIndices);
			mRawIndices++;
			mTemp = _mm_shuffle_epi8(mTemp, shufmask);
			mMin = _mm_min_epu16(mMin, mTemp);
			mMax = _mm_max_epu16(mMax, mTemp);
			_mm_store_si128(mOutputIndices, mTemp);
			mOutputIndices++;
		}

		uint16* mMaxU16 = (uint16*)&mMax;
		uint16* mMinU16 = (uint16*)&mMin;

		indexMax = std::max(indexMax, (uint32)mMaxU16[0]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[1]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[2]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[3]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[4]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[5]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[6]);
		indexMax = std::max(indexMax, (uint32)mMaxU16[7]);
		indexMin = std::min(indexMin, (uint32)mMinU16[0]);
		indexMin = std::min(indexMin, (uint32)mMinU16[1]);
		indexMin = std::min(indexMin, (uint32)mMinU16[2]);
		indexMin = std::min(indexMin, (uint32)mMinU16[3]);
		indexMin = std::min(indexMin, (uint32)mMinU16[4]);
		indexMin = std::min(indexMin, (uint32)mMinU16[5]);
		indexMin = std::min(indexMin, (uint32)mMinU16[6]);
		indexMin = std::min(indexMin, (uint32)mMinU16[7]);
	}
	uint32 _minIndex = 0xFFFFFFFF;
	uint32 _maxIndex = 0;
	for (sint32 i = countRemaining; (--i) >= 0;)
	{
		uint16 idx = _swapEndianU16(*indicesU16BE);
		*indexOutput = idx;
		indexOutput++;
		indicesU16BE++;
		_maxIndex = std::max(_maxIndex, (uint32)idx);
		_minIndex = std::min(_minIndex, (uint32)idx);
	}
	indexMax = std::max(indexMax, _maxIndex);
	indexMin = std::min(indexMin, _minIndex);
}

ATTRIBUTE_AVX2
void LatteIndices_fastConvertU32_AVX2(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	// using AVX + AVX2 we can process 8 indices at a time
	const uint32* indicesU32BE = (const uint32*)indexDataInput;
	uint32* indexOutput = (uint32*)indexDataOutput;
	sint32 count8 = count >> 3;
	sint32 countRemaining = count & 7;
	if (count8)
	{
		__m256i mMin = _mm256_set_epi32((sint32)0xFFFFFFFF, (sint32)0xFFFFFFFF, (sint32)0xFFFFFFFF, (sint32)0xFFFFFFFF, (sint32)0xFFFFFFFF, (sint32)0xFFFFFFFF, (sint32)0xFFFFFFFF, (sint32)0xFFFFFFFF);
		__m256i mMax = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, 0);
		__m256i mShuffle32Swap = _mm256_set_epi8(28,29,30,31,
			24,25,26,27,
			20,21,22,23,
			16,17,18,19,
			12,13,14,15,
			8,9,10,11,
			4,5,6,7,
			0,1,2,3);
		// unaligned
		do
		{
			__m256i mIndexData = _mm256_loadu_si256((const __m256i*)indicesU32BE);
			indicesU32BE += 8;
			_mm_prefetch((const char*)indicesU32BE, _MM_HINT_T0);
			// endian swap
			mIndexData = _mm256_shuffle_epi8(mIndexData, mShuffle32Swap);
			_mm256_store_si256((__m256i*)indexOutput, mIndexData);
			mMin = _mm256_min_epu32(mIndexData, mMin);
			mMax = _mm256_max_epu32(mIndexData, mMax);
			indexOutput += 8;
		} while (--count8);

		// fold 32 to 16 byte
		mMin = _mm256_min_epu32(mMin, _mm256_permute2x128_si256(mMin, mMin, 1));
		mMax = _mm256_max_epu32(mMax, _mm256_permute2x128_si256(mMax, mMax, 1));
		// fold 16 to 8 byte
		mMin = _mm256_min_epu32(mMin, _mm256_shuffle_epi32(mMin, (2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));
		mMax = _mm256_max_epu32(mMax, _mm256_shuffle_epi32(mMax, (2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));

		uint32* mMinU32 = (uint32*)&mMin;
		uint32* mMaxU32 = (uint32*)&mMax;

		indexMin = std::min(indexMin, (uint32)mMinU32[0]);
		indexMin = std::min(indexMin, (uint32)mMinU32[1]);

		indexMax = std::max(indexMax, (uint32)mMaxU32[0]);
		indexMax = std::max(indexMax, (uint32)mMaxU32[1]);
	}
	// process remaining indices
	uint32 _minIndex = 0xFFFFFFFF;
	uint32 _maxIndex = 0;
	for (sint32 i = countRemaining; (--i) >= 0;)
	{
		uint32 idx = _swapEndianU32(*indicesU32BE);
		*indexOutput = idx;
		indexOutput++;
		indicesU32BE++;
		_maxIndex = std::max(_maxIndex, (uint32)idx);
		_minIndex = std::min(_minIndex, (uint32)idx);
	}
	// update min/max
	indexMax = std::max(indexMax, _maxIndex);
	indexMin = std::min(indexMin, _minIndex);
}
#elif defined(__aarch64__)

void LatteIndices_fastConvertU16_NEON(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	const uint16* indicesU16BE = (const uint16*)indexDataInput;
	uint16* indexOutput = (uint16*)indexDataOutput;
	sint32 count8 = count >> 3;
	sint32 countRemaining = count & 7;

	if (count8)
	{
		uint16x8_t mMin = vdupq_n_u16(0xFFFF);
		uint16x8_t mMax = vdupq_n_u16(0x0000);
		uint16x8_t mTemp;
		uint16x8_t* mRawIndices = (uint16x8_t*) indicesU16BE;
		indicesU16BE += count8 * 8;
		uint16x8_t* mOutputIndices = (uint16x8_t*) indexOutput;
		indexOutput += count8 * 8;

		while (count8--)
		{
			mTemp = vld1q_u16((uint16*)mRawIndices);
			mRawIndices++;
			mTemp = vrev16q_u8(mTemp);
			mMin = vminq_u16(mMin, mTemp);
			mMax = vmaxq_u16(mMax, mTemp);
			vst1q_u16((uint16*)mOutputIndices, mTemp);
			mOutputIndices++;
		}

		uint16* mMaxU16 = (uint16*)&mMax;
		uint16* mMinU16 = (uint16*)&mMin;

		for (int i = 0; i < 8; ++i) {
			indexMax = std::max(indexMax, (uint32)mMaxU16[i]);
			indexMin = std::min(indexMin, (uint32)mMinU16[i]);
		}
	}
	// process remaining indices
	uint32 _minIndex = 0xFFFFFFFF;
	uint32 _maxIndex = 0;
	for (sint32 i = countRemaining; (--i) >= 0;)
	{
		uint16 idx = _swapEndianU16(*indicesU16BE);
		*indexOutput = idx;
		indexOutput++;
		indicesU16BE++;
		_maxIndex = std::max(_maxIndex, (uint32)idx);
		_minIndex = std::min(_minIndex, (uint32)idx);
	}
	// update min/max
	indexMax = std::max(indexMax, _maxIndex);
	indexMin = std::min(indexMin, _minIndex);
}

void LatteIndices_fastConvertU32_NEON(const void* indexDataInput, void* indexDataOutput, uint32 count, uint32& indexMin, uint32& indexMax)
{
	const uint32* indicesU32BE = (const uint32*)indexDataInput;
	uint32* indexOutput = (uint32*)indexDataOutput;
	sint32 count8 = count >> 2;
	sint32 countRemaining = count & 3;

	if (count8)
	{
		uint32x4_t mMin = vdupq_n_u32(0xFFFFFFFF);
		uint32x4_t mMax = vdupq_n_u32(0x00000000);
		uint32x4_t mTemp;
		uint32x4_t* mRawIndices = (uint32x4_t*) indicesU32BE;
		indicesU32BE += count8 * 4;
		uint32x4_t* mOutputIndices = (uint32x4_t*) indexOutput;
		indexOutput += count8 * 4;

		while (count8--)
		{
			mTemp = vld1q_u32((uint32*)mRawIndices);
			mRawIndices++;
			mTemp = vrev32q_u8(mTemp);
			mMin = vminq_u32(mMin, mTemp);
			mMax = vmaxq_u32(mMax, mTemp);
			vst1q_u32((uint32*)mOutputIndices, mTemp);
			mOutputIndices++;
		}

		uint32* mMaxU32 = (uint32*)&mMax;
		uint32* mMinU32 = (uint32*)&mMin;

		for (int i = 0; i < 4; ++i) {
			indexMax = std::max(indexMax, mMaxU32[i]);
			indexMin = std::min(indexMin, mMinU32[i]);
		}
	}
	// process remaining indices
	uint32 _minIndex = 0xFFFFFFFF;
	uint32 _maxIndex = 0;
	for (sint32 i = countRemaining; (--i) >= 0;)
	{
		uint32 idx = _swapEndianU32(*indicesU32BE);
		*indexOutput = idx;
		indexOutput++;
		indicesU32BE++;
		_maxIndex = std::max(_maxIndex, idx);
		_minIndex = std::min(_minIndex, idx);
	}
	// update min/max
	indexMax = std::max(indexMax, _maxIndex);
	indexMin = std::min(indexMin, _minIndex);
}

#endif

template<typename T>
void _LatteIndices_alternativeCalculateIndexMinMax(const void* indexData, uint32 count, uint32 primitiveRestartIndex, uint32& indexMin, uint32& indexMax)
{
	cemu_assert_debug(count != 0);
	const betype<T>* idxPtrT = (betype<T>*)indexData;
	T _indexMin = *idxPtrT;
	T _indexMax = *idxPtrT;
	cemu_assert_debug(primitiveRestartIndex <= std::numeric_limits<T>::max());
	T restartIndexT = (T)primitiveRestartIndex;
	while (count)
	{
		T idx = *idxPtrT;
		if (idx != restartIndexT)
		{
			_indexMin = std::min(_indexMin, idx);
			_indexMax = std::max(_indexMax, idx);
		}
		idxPtrT++;
		count--;
	}
	indexMin = _indexMin;
	indexMax = _indexMax;
}

// calculate min and max index while taking primitive restart into account
// fallback implementation in case the fast path gives us invalid results
void LatteIndices_alternativeCalculateIndexMinMax(const void* indexData, LatteIndexType indexType, uint32 count, uint32& indexMin, uint32& indexMax)
{
	if (count == 0)
	{
		indexMin = 0;
		indexMax = 0;
		return;
	}
	uint32 primitiveRestartIndex = LatteGPUState.contextNew.VGT_MULTI_PRIM_IB_RESET_INDX.get_RESTART_INDEX();

	if (indexType == LatteIndexType::U16_BE)
	{
		_LatteIndices_alternativeCalculateIndexMinMax<uint16>(indexData, count, primitiveRestartIndex, indexMin, indexMax);
	}
	else if (indexType == LatteIndexType::U32_BE)
	{
		_LatteIndices_alternativeCalculateIndexMinMax<uint32>(indexData, count, primitiveRestartIndex, indexMin, indexMax);
	}
	else
	{
		cemu_assert_debug(false);
	}
}

void LatteIndices_decode(const void* indexData, LatteIndexType indexType, uint32 count, LattePrimitiveMode primitiveMode, uint32& indexMin, uint32& indexMax, Renderer::INDEX_TYPE& renderIndexType, uint32& outputCount, Renderer::IndexAllocation& indexAllocation)
{
	// what this should do:
	// [x] use fast SIMD-based index decoding
	// [x] unpack QUAD indices to triangle indices
	// [x] calculate min and max index, be careful about primitive restart index
	// [x] decode data directly into coherent memory buffer?
	// [ ] better cache implementation, allow to cache across frames

	// reuse from cache if data didn't change
	auto cacheEntry = std::find_if(LatteIndexCache.entry.begin(), LatteIndexCache.entry.end(), [indexData, count, primitiveMode, indexType](const auto& entry)
	{
		return entry.lastPtr == indexData && entry.lastCount == count && entry.lastPrimitiveMode == primitiveMode && entry.lastIndexType == indexType;
	});
	if (cacheEntry != LatteIndexCache.entry.end())
	{
		indexMin = cacheEntry->indexMin;
		indexMax = cacheEntry->indexMax;
		renderIndexType = cacheEntry->renderIndexType;
		outputCount = cacheEntry->outputCount;
		indexAllocation = cacheEntry->indexAllocation;
		cacheEntry->lastUsed = LatteIndices_GetNextUsageIndex();
		return;
	}

	outputCount = 0;
	if (indexType == LatteIndexType::AUTO)
		renderIndexType = Renderer::INDEX_TYPE::NONE;
	else if (indexType == LatteIndexType::U16_BE || indexType == LatteIndexType::U16_LE)
		renderIndexType = Renderer::INDEX_TYPE::U16;
	else if (indexType == LatteIndexType::U32_BE)
		renderIndexType = Renderer::INDEX_TYPE::U32;
	else
		cemu_assert_debug(false);

	uint32 primitiveRestartIndex = LatteGPUState.contextNew.VGT_MULTI_PRIM_IB_RESET_INDX.get_RESTART_INDEX();

	// calculate index output size
	uint32 indexOutputSize = LatteIndices_calculateIndexOutputSize(primitiveMode, indexType, count);
	if (indexOutputSize == 0)
	{
		outputCount = count;
		indexMin = 0;
		indexMax = std::max(count, 1u)-1;
		renderIndexType = Renderer::INDEX_TYPE::NONE;
		indexAllocation = {};
		return; // no indices
	}
	// query index buffer from renderer
	indexAllocation = g_renderer->indexData_reserveIndexMemory(indexOutputSize);
	void* indexOutputPtr = indexAllocation.mem;

	// decode indices
	indexMin = std::numeric_limits<uint32>::max();
	indexMax = std::numeric_limits<uint32>::min();
	if (primitiveMode == LattePrimitiveMode::QUADS)
	{
		// unpack quads into triangles
		if (indexType == LatteIndexType::AUTO)
		{
			if (count <= 0xFFFF)
			{
				LatteIndices_generateAutoQuadIndices<uint16>(indexData, indexOutputPtr, count, indexMin, indexMax);
				renderIndexType = Renderer::INDEX_TYPE::U16;
			}
			else
			{
				LatteIndices_generateAutoQuadIndices<uint32>(indexData, indexOutputPtr, count, indexMin, indexMax);
				renderIndexType = Renderer::INDEX_TYPE::U32;
			}
		}
		else if (indexType == LatteIndexType::U16_BE)
			LatteIndices_unpackQuadsAndConvert<uint16>(indexData, indexOutputPtr, count, indexMin, indexMax);
		else if (indexType == LatteIndexType::U32_BE)
			LatteIndices_unpackQuadsAndConvert<uint32>(indexData, indexOutputPtr, count, indexMin, indexMax);
		else
			cemu_assert_debug(false);
		outputCount = count / 4 * 6;
	}
	else if (primitiveMode == LattePrimitiveMode::QUAD_STRIP)
	{
		// unpack quad strip into triangles
		if (indexType == LatteIndexType::AUTO)
		{
			if (count <= 0xFFFF)
			{
				LatteIndices_generateAutoQuadStripIndices<uint16>(indexOutputPtr, count, indexMin, indexMax);
				renderIndexType = Renderer::INDEX_TYPE::U16;
			}
			else
			{
				LatteIndices_generateAutoQuadStripIndices<uint32>(indexOutputPtr, count, indexMin, indexMax);
				renderIndexType = Renderer::INDEX_TYPE::U32;
			}
		}
		else if (indexType == LatteIndexType::U16_BE)
			LatteIndices_unpackQuadStripAndConvert<uint16>(indexData, indexOutputPtr, count, indexMin, indexMax);
		else if (indexType == LatteIndexType::U32_BE)
			LatteIndices_unpackQuadStripAndConvert<uint32>(indexData, indexOutputPtr, count, indexMin, indexMax);
		else
			cemu_assert_debug(false);
		if (count >= 2)
			outputCount = (count - 2) / 2 * 6;
		else
			outputCount = 0;
	}
	else if (primitiveMode == LattePrimitiveMode::LINE_LOOP)
	{
		// unpack line loop into line strip with extra reconnecting vertex
		if (indexType == LatteIndexType::AUTO)
		{
			if (count <= 0xFFFF)
			{
				LatteIndices_generateAutoLineLoopIndices<uint16>(indexOutputPtr, count, indexMin, indexMax);
				renderIndexType = Renderer::INDEX_TYPE::U16;
			}
			else
			{
				LatteIndices_generateAutoLineLoopIndices<uint32>(indexOutputPtr, count, indexMin, indexMax);
				renderIndexType = Renderer::INDEX_TYPE::U32;
			}
		}
		else if (indexType == LatteIndexType::U16_BE)
			LatteIndices_unpackLineLoopAndConvert<uint16>(indexData, indexOutputPtr, count, indexMin, indexMax);
		else if (indexType == LatteIndexType::U32_BE)
			LatteIndices_unpackLineLoopAndConvert<uint32>(indexData, indexOutputPtr, count, indexMin, indexMax);
		else
			cemu_assert_debug(false);
		outputCount = count + 1;
	}
	else
	{
		if (indexType == LatteIndexType::U16_BE)
		{
#if defined(ARCH_X86_64)
			if (g_CPUFeatures.x86.avx2)
				LatteIndices_fastConvertU16_AVX2(indexData, indexOutputPtr, count, indexMin, indexMax);
			else if (g_CPUFeatures.x86.sse4_1 && g_CPUFeatures.x86.ssse3)
				LatteIndices_fastConvertU16_SSE41(indexData, indexOutputPtr, count, indexMin, indexMax);
			else
				LatteIndices_convertBE<uint16>(indexData, indexOutputPtr, count, indexMin, indexMax);
#elif defined(__aarch64__)
			LatteIndices_fastConvertU16_NEON(indexData, indexOutputPtr, count, indexMin, indexMax);
#else
			LatteIndices_convertBE<uint16>(indexData, indexOutputPtr, count, indexMin, indexMax);            
#endif
		}
		else if (indexType == LatteIndexType::U32_BE)
		{
#if defined(ARCH_X86_64)
			if (g_CPUFeatures.x86.avx2)
				LatteIndices_fastConvertU32_AVX2(indexData, indexOutputPtr, count, indexMin, indexMax);
			else
				LatteIndices_convertBE<uint32>(indexData, indexOutputPtr, count, indexMin, indexMax);
#elif defined(__aarch64__)
			LatteIndices_fastConvertU32_NEON(indexData, indexOutputPtr, count, indexMin, indexMax);
#else
			LatteIndices_convertBE<uint32>(indexData, indexOutputPtr, count, indexMin, indexMax);            
#endif
		}
		else if (indexType == LatteIndexType::U16_LE)
		{
			LatteIndices_convertLE<uint16>(indexData, indexOutputPtr, count, indexMin, indexMax);
		}
		else if (indexType == LatteIndexType::U32_LE)
		{
			LatteIndices_convertLE<uint32>(indexData, indexOutputPtr, count, indexMin, indexMax);
		}
		else
			cemu_assert_debug(false);
		outputCount = count;
	}
	// the above algorithms use a simplistic approach to get indexMin/indexMax
	// here we make sure primitive restart indices dont influence the index range
	if (primitiveRestartIndex == indexMin || primitiveRestartIndex == indexMax)
	{
		// recalculate index range but filter out primitive restart index
		LatteIndices_alternativeCalculateIndexMinMax(indexData, indexType, count, indexMin, indexMax);
	}
	g_renderer->indexData_uploadIndexMemory(indexAllocation);
	performanceMonitor.cycle[performanceMonitor.cycleIndex].indexDataUploaded += indexOutputSize;
	// get least recently used cache entry
	auto lruEntry = std::min_element(LatteIndexCache.entry.begin(), LatteIndexCache.entry.end(), [](const auto& a, const auto& b)
	{
		return a.lastUsed < b.lastUsed;
	});
	// invalidate previous allocation
	if(lruEntry->lastPtr != nullptr)
		g_renderer->indexData_releaseIndexMemory(lruEntry->indexAllocation);
	// update cache
	lruEntry->lastPtr = indexData;
	lruEntry->lastCount = count;
	lruEntry->lastPrimitiveMode = primitiveMode;
	lruEntry->lastIndexType = indexType;
	lruEntry->indexMin = indexMin;
	lruEntry->indexMax = indexMax;
	lruEntry->renderIndexType = renderIndexType;
	lruEntry->outputCount = outputCount;
	lruEntry->indexAllocation = indexAllocation;
	lruEntry->lastUsed = LatteIndices_GetNextUsageIndex();
}
