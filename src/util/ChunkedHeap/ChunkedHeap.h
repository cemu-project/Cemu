#pragma once

struct CHAddr
{
	uint32 offset;
	uint32 chunkIndex;

	CHAddr(uint32 _offset, uint32 _chunkIndex) : offset(_offset), chunkIndex(_chunkIndex) {};
	CHAddr() : offset(0xFFFFFFFF), chunkIndex(0xFFFFFFFF) {};

	bool isValid() { return chunkIndex != 0xFFFFFFFF; };
	static CHAddr getInvalid() { return CHAddr(0xFFFFFFFF, 0xFFFFFFFF); };
};

class ChunkedHeap
{
	struct allocRange_t
	{
		allocRange_t* nextFree{};
		allocRange_t* prevFree{};
		allocRange_t* prevOrdered{};
		allocRange_t* nextOrdered{};
		uint32 offset;
		uint32 chunkIndex;
		uint32 size;
		bool isFree;
		allocRange_t(uint32 _offset, uint32 _chunkIndex, uint32 _size, bool _isFree) : offset(_offset), chunkIndex(_chunkIndex), size(_size), isFree(_isFree), nextFree(nullptr) {};
	};

	struct chunk_t
	{
		std::unordered_map<uint32, allocRange_t*> map_allocatedRange;
	};

public:
	ChunkedHeap()
	{
	}

	CHAddr alloc(uint32 size, uint32 alignment = 4)
	{
		return _alloc(size, alignment);
	}

	void free(CHAddr addr)
	{
		_free(addr);
	}

	virtual uint32 allocateNewChunk(uint32 chunkIndex, uint32 minimumAllocationSize)
	{
		return 0;
	}

private:
	unsigned ulog2(uint32 v)
	{
		static const unsigned MUL_DE_BRUIJN_BIT[] =
		{
		   0,  9,  1, 10, 13, 21,  2, 29, 11, 14, 16, 18, 22, 25,  3, 30,
		   8, 12, 20, 28, 15, 17, 24,  7, 19, 27, 23,  6, 26,  5,  4, 31
		};

		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;

		return MUL_DE_BRUIJN_BIT[(v * 0x07C4ACDDu) >> 27];
	}

	void trackFreeRange(allocRange_t* range)
	{
		// get index of msb
		cemu_assert_debug(range->size != 0); // size of zero is not allowed
		uint32 bucketIndex = ulog2(range->size);
		range->nextFree = bucketFreeRange[bucketIndex];
		if (bucketFreeRange[bucketIndex])
			bucketFreeRange[bucketIndex]->prevFree = range;
		range->prevFree = nullptr;
		bucketFreeRange[bucketIndex] = range;
	}

	void forgetFreeRange(allocRange_t* range, uint32 bucketIndex)
	{
		allocRange_t* prevRange = range->prevFree;
		allocRange_t* nextRange = range->nextFree;
		if (prevRange)
		{
			prevRange->nextFree = nextRange;
			if (nextRange)
				nextRange->prevFree = prevRange;
		}
		else
		{
			if (bucketFreeRange[bucketIndex] != range)
				assert_dbg();
			bucketFreeRange[bucketIndex] = nextRange;
			if (nextRange)
				nextRange->prevFree = nullptr;
		}
	}

	bool allocateChunk(uint32 minimumAllocationSize)
	{
		uint32 chunkIndex = (uint32)list_chunks.size();
		list_chunks.emplace_back(new chunk_t());
		uint32 chunkSize = allocateNewChunk(chunkIndex, minimumAllocationSize);
		if (chunkSize == 0)
			return false;
		allocRange_t* range = new allocRange_t(0, chunkIndex, chunkSize, true);
		trackFreeRange(range);
		numHeapBytes += chunkSize;
		return true;
	}

	void _allocFrom(allocRange_t* range, uint32 bucketIndex, uint32 allocOffset, uint32 allocSize)
	{
		// remove the range from the chain of free ranges
		forgetFreeRange(range, bucketIndex);
		// split head, allocation and tail into separate ranges
		if (allocOffset > range->offset)
		{
			// alignment padding -> create free range
			allocRange_t* head = new allocRange_t(range->offset, range->chunkIndex, allocOffset - range->offset, true);
			trackFreeRange(head);
			if (range->prevOrdered)
				range->prevOrdered->nextOrdered = head;
			head->prevOrdered = range->prevOrdered;
			head->nextOrdered = range;
			range->prevOrdered = head;
		}
		if ((allocOffset + allocSize) < (range->offset + range->size)) // todo - create only if it's more than a couple of bytes?
		{
			// tail -> create free range
			allocRange_t* tail = new allocRange_t((allocOffset + allocSize), range->chunkIndex, (range->offset + range->size) - (allocOffset + allocSize), true);
			trackFreeRange(tail);
			if (range->nextOrdered)
				range->nextOrdered->prevOrdered = tail;
			tail->prevOrdered = range;
			tail->nextOrdered = range->nextOrdered;
			range->nextOrdered = tail;
		}
		range->offset = allocOffset;
		range->size = allocSize;
		range->isFree = false;
	}

	CHAddr _alloc(uint32 size, uint32 alignment)
	{
		// find smallest bucket to scan
		uint32 alignmentM1 = alignment - 1;
		uint32 bucketIndex = ulog2(size);
		while (bucketIndex < 32)
		{
			allocRange_t* range = bucketFreeRange[bucketIndex];
			while (range)
			{
				if (range->size >= size)
				{
					// verify if aligned allocation fits
					uint32 alignedOffset = (range->offset + alignmentM1) & ~alignmentM1;
					uint32 alignmentLoss = alignedOffset - range->offset;
					if (alignmentLoss < range->size && (range->size - alignmentLoss) >= size)
					{
						_allocFrom(range, bucketIndex, alignedOffset, size);
						list_chunks[range->chunkIndex]->map_allocatedRange.emplace(alignedOffset, range);
						numAllocatedBytes += size;
						return CHAddr(alignedOffset, range->chunkIndex);
					}
				}
				range = range->nextFree;
			}
			bucketIndex++; // try higher bucket
		}
		if(allocationLimitReached)
			return CHAddr(0xFFFFFFFF, 0xFFFFFFFF);
		if (!allocateChunk(size))
		{
			allocationLimitReached = true;
			return CHAddr(0xFFFFFFFF, 0xFFFFFFFF);
		}
		return _alloc(size, alignment);
	}

	void _free(CHAddr addr)
	{
		auto it = list_chunks[addr.chunkIndex]->map_allocatedRange.find(addr.offset);
		if (it == list_chunks[addr.chunkIndex]->map_allocatedRange.end())
		{
			cemuLog_log(LogType::Force, "Internal heap error. {:08x} {:08x}", addr.chunkIndex, addr.offset);
			cemuLog_log(LogType::Force, "Debug info:");
			for (auto& rangeItr : list_chunks[addr.chunkIndex]->map_allocatedRange)
			{
				cemuLog_log(LogType::Force, "{:08x} {:08x}", rangeItr.second->offset, rangeItr.second->size);
			}
			return;
		}

		allocRange_t* range = it->second;
		numAllocatedBytes -= it->second->size;
		list_chunks[range->chunkIndex]->map_allocatedRange.erase(it);
		// try merge left or right
		allocRange_t* prevRange = range->prevOrdered;
		allocRange_t* nextRange = range->nextOrdered;
		if (prevRange && prevRange->isFree)
		{
			if (nextRange && nextRange->isFree)
			{
				forgetFreeRange(nextRange, ulog2(nextRange->size));
				uint32 newSize = (nextRange->offset + nextRange->size) - prevRange->offset;
				prevRange->nextOrdered = nextRange->nextOrdered;
				if (nextRange->nextOrdered)
					nextRange->nextOrdered->prevOrdered = prevRange;
				forgetFreeRange(prevRange, ulog2(prevRange->size));
				prevRange->size = newSize;
				trackFreeRange(prevRange);
				delete range;
				delete nextRange;
			}
			else
			{
				uint32 newSize = (range->offset + range->size) - prevRange->offset;
				prevRange->nextOrdered = nextRange;
				if (nextRange)
					nextRange->prevOrdered = prevRange;
				forgetFreeRange(prevRange, ulog2(prevRange->size));
				prevRange->size = newSize;
				trackFreeRange(prevRange);
				delete range;
			}
		}
		else if (nextRange && nextRange->isFree)
		{
			uint32 newOffset = range->offset;
			uint32 newSize = (nextRange->offset + nextRange->size) - newOffset;
			forgetFreeRange(nextRange, ulog2(nextRange->size));
			nextRange->offset = newOffset;
			nextRange->size = newSize;
			if (range->prevOrdered)
				range->prevOrdered->nextOrdered = nextRange;
			nextRange->prevOrdered = range->prevOrdered;
			trackFreeRange(nextRange);
			delete range;
		}
		else
		{
			range->isFree = true;
			trackFreeRange(range);
		}
	}

	void verifyHeap()
	{
		// check for collisions within bucketFreeRange
		struct availableRange_t
		{
			uint32 chunkIndex;
			uint32 offset;
			uint32 size;
		};

		std::vector<availableRange_t> availRanges;

		for (uint32 i = 0; i < 32; i++)
		{
			allocRange_t* ar = bucketFreeRange[i];
			while (ar)
			{
				availableRange_t dbgRange;
				dbgRange.chunkIndex = ar->chunkIndex;
				dbgRange.offset = ar->offset;
				dbgRange.size = ar->size;

				for (auto& itr : availRanges)
				{
					if (itr.chunkIndex != dbgRange.chunkIndex)
						continue;
					if (itr.offset < (dbgRange.offset + dbgRange.size) && (itr.offset + itr.size) >(dbgRange.offset))
						assert_dbg();
				}

				availRanges.emplace_back(dbgRange);

				ar = ar->nextFree;
			}
		}

	}

private:
	std::vector<chunk_t*> list_chunks;
	allocRange_t* bucketFreeRange[32]{};
	bool allocationLimitReached = false;

public:
	// statistics
	uint32 numHeapBytes{}; // total size of the heap
	uint32 numAllocatedBytes{};
};

class VGenericHeap
{
public:
	virtual void* alloc(uint32 size, uint32 alignment) = 0;
	virtual void free(void* addr) = 0;
};

class VHeap : public VGenericHeap
{
	struct allocRange_t
	{
		allocRange_t* nextFree{};
		allocRange_t* prevFree{};
		allocRange_t* prevOrdered{};
		allocRange_t* nextOrdered{};
		uint32 offset;
		uint32 size;
		bool isFree;
		allocRange_t(uint32 _offset, uint32 _size, bool _isFree) : offset(_offset), size(_size), isFree(_isFree), nextFree(nullptr) {};
	};

	struct chunk_t
	{
		std::unordered_map<uint32, allocRange_t*> map_allocatedRange;
	};

public:
	VHeap(void* heapBase, uint32 heapSize) : m_heapBase((uint8*)heapBase), m_heapSize(heapSize)
	{
		allocRange_t* range = new allocRange_t(0, heapSize, true);
		trackFreeRange(range);
	}

	~VHeap()
	{
		for (auto freeRange : bucketFreeRange)
		{
			while (freeRange)
			{
				auto temp = freeRange;
				freeRange = freeRange->nextFree;
				delete temp;
			}
		}
	}

	void setHeapBase(void* heapBase)
	{
		cemu_assert_debug(map_allocatedRange.empty()); // heap base can only be changed when there are no active allocations
		m_heapBase = (uint8*)heapBase;
	}

	void* alloc(uint32 size, uint32 alignment = 4) override
	{
		cemu_assert_debug(m_heapBase != nullptr); // if this is null, we cant use alloc() == nullptr to determine if an allocation failed
		uint32 allocOffset = 0;
		bool r = _alloc(size, alignment, allocOffset);
		if (!r)
			return nullptr;
		return m_heapBase + allocOffset;
	}

	void free(void* addr) override
	{
		_free((uint32)((uint8*)addr - (uint8*)m_heapBase));
	}

	bool allocOffset(uint32 size, uint32 alignment, uint32& offsetOut)
	{
		uint32 allocOffset = 0;
		bool r = _alloc(size, alignment, allocOffset);
		if (!r)
			return false;
		offsetOut = allocOffset;
		return true;
	}

	void freeOffset(uint32 offset)
	{
		_free((uint32)offset);
	}

	uint32 getAllocationSizeFromAddr(void* addr)
	{
		uint32 addrOffset = (uint32)((uint8*)addr - m_heapBase);
		auto it = map_allocatedRange.find(addrOffset);
		if (it == map_allocatedRange.end())
			assert_dbg();
		return it->second->size;
	}

	bool hasAllocations()
	{
		return !map_allocatedRange.empty();
	}

	void getStats(uint32& heapSize, uint32& allocationSize, uint32& allocNum)
	{
		heapSize = m_heapSize;
		allocationSize = m_statsMemAllocated;
		allocNum = (uint32)map_allocatedRange.size();
	}

private:
	unsigned ulog2(uint32 v)
	{
		static const unsigned MUL_DE_BRUIJN_BIT[] =
		{
		   0,  9,  1, 10, 13, 21,  2, 29, 11, 14, 16, 18, 22, 25,  3, 30,
		   8, 12, 20, 28, 15, 17, 24,  7, 19, 27, 23,  6, 26,  5,  4, 31
		};

		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;

		return MUL_DE_BRUIJN_BIT[(v * 0x07C4ACDDu) >> 27];
	}

	void trackFreeRange(allocRange_t* range)
	{
		// get index of msb
		if (range->size == 0)
			assert_dbg(); // not allowed
		uint32 bucketIndex = ulog2(range->size);
		range->nextFree = bucketFreeRange[bucketIndex];
		if (bucketFreeRange[bucketIndex])
			bucketFreeRange[bucketIndex]->prevFree = range;
		range->prevFree = nullptr;
		bucketFreeRange[bucketIndex] = range;
	}

	void forgetFreeRange(allocRange_t* range, uint32 bucketIndex)
	{
		allocRange_t* prevRange = range->prevFree;
		allocRange_t* nextRange = range->nextFree;
		if (prevRange)
		{
			prevRange->nextFree = nextRange;
			if (nextRange)
				nextRange->prevFree = prevRange;
		}
		else
		{
			if (bucketFreeRange[bucketIndex] != range)
				assert_dbg();
			bucketFreeRange[bucketIndex] = nextRange;
			if (nextRange)
				nextRange->prevFree = nullptr;
		}
	}

	void _allocFrom(allocRange_t* range, uint32 bucketIndex, uint32 allocOffset, uint32 allocSize)
	{
		// remove the range from the chain of free ranges
		forgetFreeRange(range, bucketIndex);
		// split head, allocation and tail into separate ranges
		if (allocOffset > range->offset)
		{
			// alignment padding -> create free range
			allocRange_t* head = new allocRange_t(range->offset, allocOffset - range->offset, true);
			trackFreeRange(head);
			if (range->prevOrdered)
				range->prevOrdered->nextOrdered = head;
			head->prevOrdered = range->prevOrdered;
			head->nextOrdered = range;
			range->prevOrdered = head;
		}
		if ((allocOffset + allocSize) < (range->offset + range->size)) // todo - create only if it's more than a couple of bytes?
		{
			// tail -> create free range
			allocRange_t* tail = new allocRange_t((allocOffset + allocSize), (range->offset + range->size) - (allocOffset + allocSize), true);
			trackFreeRange(tail);
			if (range->nextOrdered)
				range->nextOrdered->prevOrdered = tail;
			tail->prevOrdered = range;
			tail->nextOrdered = range->nextOrdered;
			range->nextOrdered = tail;
		}
		range->offset = allocOffset;
		range->size = allocSize;
		range->isFree = false;
		m_statsMemAllocated += allocSize;
	}

	bool _alloc(uint32 size, uint32 alignment, uint32& allocOffsetOut)
	{
		if(size == 0)
		{
			size = 1; // zero-sized allocations are not supported
			cemu_assert_suspicious();
		}
		// find smallest bucket to scan
		uint32 alignmentM1 = alignment - 1;
		uint32 bucketIndex = ulog2(size);
		while (bucketIndex < 32)
		{
			allocRange_t* range = bucketFreeRange[bucketIndex];
			while (range)
			{
				if (range->size >= size)
				{
					// verify if aligned allocation fits
					uint32 alignedOffset = (range->offset + alignmentM1) & ~alignmentM1;
					uint32 alignmentLoss = alignedOffset - range->offset;
					if (alignmentLoss < range->size && (range->size - alignmentLoss) >= size)
					{
						_allocFrom(range, bucketIndex, alignedOffset, size);
						map_allocatedRange.emplace(alignedOffset, range);
						allocOffsetOut = alignedOffset;
						return true;
					}
				}
				range = range->nextFree;
			}
			bucketIndex++; // try higher bucket
		}
		return false;
	}

	void _free(uint32 addrOffset)
	{
		auto it = map_allocatedRange.find(addrOffset);
		if (it == map_allocatedRange.end())
		{
			cemuLog_log(LogType::Force, "VHeap internal error");
			cemu_assert(false);
		}
		allocRange_t* range = it->second;
		map_allocatedRange.erase(it);
		m_statsMemAllocated -= range->size;
		// try merge left or right
		allocRange_t* prevRange = range->prevOrdered;
		allocRange_t* nextRange = range->nextOrdered;
		if (prevRange && prevRange->isFree)
		{
			if (nextRange && nextRange->isFree)
			{
				forgetFreeRange(nextRange, ulog2(nextRange->size));
				uint32 newSize = (nextRange->offset + nextRange->size) - prevRange->offset;
				prevRange->nextOrdered = nextRange->nextOrdered;
				if (nextRange->nextOrdered)
					nextRange->nextOrdered->prevOrdered = prevRange;
				forgetFreeRange(prevRange, ulog2(prevRange->size));
				prevRange->size = newSize;
				trackFreeRange(prevRange);
				delete range;
				delete nextRange;
			}
			else
			{
				uint32 newSize = (range->offset + range->size) - prevRange->offset;
				prevRange->nextOrdered = nextRange;
				if (nextRange)
					nextRange->prevOrdered = prevRange;
				forgetFreeRange(prevRange, ulog2(prevRange->size));
				prevRange->size = newSize;
				trackFreeRange(prevRange);
				delete range;
			}
		}
		else if (nextRange && nextRange->isFree)
		{
			uint32 newOffset = range->offset;
			uint32 newSize = (nextRange->offset + nextRange->size) - newOffset;
			forgetFreeRange(nextRange, ulog2(nextRange->size));
			nextRange->offset = newOffset;
			nextRange->size = newSize;
			if (range->prevOrdered)
				range->prevOrdered->nextOrdered = nextRange;
			nextRange->prevOrdered = range->prevOrdered;
			trackFreeRange(nextRange);
			delete range;
		}
		else
		{
			range->isFree = true;
			trackFreeRange(range);
		}
	}

private:
	allocRange_t* bucketFreeRange[32]{};
	std::unordered_map<uint32, allocRange_t*> map_allocatedRange;
	uint8* m_heapBase;
	const uint32 m_heapSize;
	uint32 m_statsMemAllocated{ 0 };
};

template<uint32 TChunkSize>
class ChunkedFlatAllocator
{
public:
	void setBaseAllocator(VGenericHeap* baseHeap)
	{
		m_currentBaseAllocator = baseHeap;
	}

	void* alloc(uint32 size, uint32 alignment = 4)
	{
		if (m_currentBlockPtr)
		{
			m_currentBlockOffset = (m_currentBlockOffset + alignment - 1) & ~(alignment - 1);
			if ((m_currentBlockOffset+size) <= TChunkSize)
			{
				void* allocPtr = m_currentBlockPtr + m_currentBlockOffset;
				m_currentBlockOffset += size;
				return allocPtr;
			}
		}
		allocateAdditionalChunk();
		return alloc(size, alignment);
	}

	void releaseAll()
	{
		for (auto it : m_allocatedBlocks)
			m_currentBaseAllocator->free(it);
		m_allocatedBlocks.clear();
		m_currentBlockPtr = nullptr;
		m_currentBlockOffset = 0;
	}

	void forEachBlock(void(*funcCb)(void* mem, uint32 size))
	{
		for (auto it : m_allocatedBlocks)
			funcCb(it, TChunkSize);
	}

	uint32 getCurrentBlockOffset() const { return m_currentBlockOffset; }
	uint8* getCurrentBlockPtr() const { return m_currentBlockPtr; }
	
private:
	void allocateAdditionalChunk()
	{
		m_currentBlockPtr = (uint8*)m_currentBaseAllocator->alloc(TChunkSize, 256);
		m_currentBlockOffset = 0;
		m_allocatedBlocks.emplace_back(m_currentBlockPtr);
	}

	VGenericHeap* m_currentBaseAllocator{};
	uint8* m_currentBlockPtr{};
	uint32 m_currentBlockOffset{};
	std::vector<void*> m_allocatedBlocks;
};
