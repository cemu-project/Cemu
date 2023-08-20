#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "util/ChunkedHeap/ChunkedHeap.h"
#include "util/helpers/fspinlock.h"
#include "config/ActiveSettings.h"

#define CACHE_PAGE_SIZE		0x400
#define CACHE_PAGE_SIZE_M1	(CACHE_PAGE_SIZE-1)

uint32 g_currentCacheChronon = 0;

template<typename TRangeData, typename TNodeObject>
class IntervalTree2
{
	// TNodeObject will be interfaced with via callbacks to static methods

	// static TNodeObject* Create(TRangeData rangeBegin, TRangeData rangeEnd, std::span<TNodeObject*> overlappingObjects)
	// Create a new node with the given range. overlappingObjects contains all the nodes that are replaced by this operation. The callee has to delete all objects in overlappingObjects (Delete callback wont be invoked)

	// static void Delete(TNodeObject* nodeObject)
	// Delete a node object. Replacement operations won't trigger this callback and instead pass the objects to Create()

	// static void Resize(TNodeObject* nodeObject, TRangeData rangeBegin, TRangeData rangeEnd)
	// Shrink or extend an existing range

	// static TNodeObject* Split(TNodeObject* nodeObject, TRangeData firstRangeBegin, TRangeData firstRangeEnd, TRangeData secondRangeBegin, TRangeData secondRangeEnd)
	// Cut a hole into an existing range and split it in two. Should return the newly created node object after the hole

	static_assert(std::is_pointer<TNodeObject>::value == false, "TNodeObject must be a non-pointer type");

	struct InternalRange
	{
		InternalRange() = default;
		InternalRange(TRangeData _rangeBegin, TRangeData _rangeEnd) : rangeBegin(_rangeBegin), rangeEnd(_rangeEnd) { cemu_assert_debug(_rangeBegin < _rangeEnd); };

		TRangeData rangeBegin;
		TRangeData rangeEnd;

		bool operator<(const InternalRange& rhs) const
		{
			// use <= instead of < because ranges are allowed to touch (e.g. 10-20 and 20-30 dont get merged)
			return this->rangeEnd <= rhs.rangeBegin;
		}

	};

	std::map<InternalRange, TNodeObject*> m_map;
	std::vector<TNodeObject*> m_tempObjectArray;

public:
	TNodeObject* getRange(TRangeData rangeBegin, TRangeData rangeEnd)
	{
		auto itr = m_map.find(InternalRange(rangeBegin, rangeEnd));
		if (itr == m_map.cend())
			return nullptr;
		if (rangeBegin < (*itr).first.rangeBegin)
			return nullptr;
		if (rangeEnd > (*itr).first.rangeEnd)
			return nullptr;
		return (*itr).second;
	}

	TNodeObject* getRangeByPoint(TRangeData rangeOffset)
	{
		auto itr = m_map.find(InternalRange(rangeOffset, rangeOffset+1)); // todo - better to use custom comparator instead of +1?
		if (itr == m_map.cend())
			return nullptr;
		cemu_assert_debug(rangeOffset >= (*itr).first.rangeBegin);
		cemu_assert_debug(rangeOffset < (*itr).first.rangeEnd);
		return (*itr).second;
	}

	void addRange(TRangeData rangeBegin, TRangeData rangeEnd)
	{
		if (rangeEnd == rangeBegin)
			return;
		InternalRange range(rangeBegin, rangeEnd);
		auto itr = m_map.find(range);
		if (itr == m_map.cend())
		{
			// new entry
			m_map.emplace(range, TNodeObject::Create(rangeBegin, rangeEnd, std::span<TNodeObject*>()));
		}
		else
		{
			// overlap detected
			if (rangeBegin >= (*itr).first.rangeBegin && rangeEnd <= (*itr).first.rangeEnd)
				return; // do nothing if added range is already covered
			rangeBegin = (std::min)(rangeBegin, (*itr).first.rangeBegin);
			// DEBUG - make sure this is the start point of the merge process (the first entry that starts below minValue)
#ifdef CEMU_DEBUG_ASSERT
			if (itr != m_map.cbegin())
			{
				// check previous result
				auto itrCopy = itr;
				--itrCopy;
				if ((*itrCopy).first.rangeEnd > rangeBegin)
				{
					assert_dbg(); // n-1 entry is also overlapping
					rangeBegin = (std::min)(rangeBegin, (*itrCopy).first.rangeBegin);
				}
			}
#endif
			// DEBUG - END
			// collect and remove all overlapping ranges
			size_t count = 0;
			while (itr != m_map.cend() && (*itr).first.rangeBegin < rangeEnd)
			{
				rangeEnd = (std::max)(rangeEnd, (*itr).first.rangeEnd);
				if (m_tempObjectArray.size() <= count)
					m_tempObjectArray.resize(count + 8);
				m_tempObjectArray[count] = (*itr).second;
				count++;
				auto tempItr = itr;
				++itr;
				m_map.erase(tempItr);
			}

			// create callback
			TNodeObject* newObject = TNodeObject::Create(rangeBegin, rangeEnd, std::span<TNodeObject*>(m_tempObjectArray.data(), count));
			m_map.emplace(InternalRange(rangeBegin, rangeEnd), newObject);
		}
	}

	void removeRange(TRangeData rangeBegin, TRangeData rangeEnd)
	{
		InternalRange range(rangeBegin, rangeEnd);
		auto itr = m_map.find(range);
		if (itr == m_map.cend())
			return;
		cemu_assert_debug(itr == m_map.lower_bound(range));
		while (itr != m_map.cend() && (*itr).first.rangeBegin < rangeEnd)
		{
			if ((*itr).first.rangeBegin >= rangeBegin && (*itr).first.rangeEnd <= rangeEnd)
			{
				// delete entire range
				auto itrCopy = itr;
				TNodeObject* t = (*itr).second;
				++itr;
				m_map.erase(itrCopy);
				TNodeObject::Delete(t);
				continue;
			}
			if (rangeBegin > (*itr).first.rangeBegin && rangeEnd < (*itr).first.rangeEnd)
			{
				// cut hole into existing range
				TRangeData firstRangeBegin = (*itr).first.rangeBegin;
				TRangeData firstRangeEnd = rangeBegin;
				TRangeData secondRangeBegin = rangeEnd;
				TRangeData secondRangeEnd = (*itr).first.rangeEnd;
				TNodeObject* newObject = TNodeObject::Split((*itr).second, firstRangeBegin, firstRangeEnd, secondRangeBegin, secondRangeEnd);
				// modify key
				auto nh = m_map.extract(itr);
				nh.key().rangeBegin = firstRangeBegin;
				nh.key().rangeEnd = firstRangeEnd;
				m_map.insert(std::move(nh));
				// insert new object after hole
				m_map.emplace(InternalRange(secondRangeBegin, secondRangeEnd), newObject);
				return; // done
			}
			// shrink (trim either beginning or end)
			TRangeData newRangeBegin;
			TRangeData newRangeEnd;
			if ((rangeBegin <= (*itr).first.rangeBegin && rangeEnd < (*itr).first.rangeEnd))
			{
				// trim from beginning
				newRangeBegin = (std::max)((*itr).first.rangeBegin, rangeEnd);
				newRangeEnd = (*itr).first.rangeEnd;
			}
			else if ((rangeBegin > (*itr).first.rangeBegin && rangeEnd >= (*itr).first.rangeEnd))
			{
				// trim from end
				newRangeBegin = (*itr).first.rangeBegin;
				newRangeEnd = (std::min)((*itr).first.rangeEnd, rangeBegin);
			}
			else
			{
				assert_dbg(); // should not happen
			}
			TNodeObject::Resize((*itr).second, newRangeBegin, newRangeEnd);
			// modify key and increment iterator
			auto itrCopy = itr;
			++itr;
			auto nh = m_map.extract(itrCopy);
			nh.key().rangeBegin = newRangeBegin;
			nh.key().rangeEnd = newRangeEnd;
			m_map.insert(std::move(nh));
		}
	}

	// remove existing range that matches given begin and end
	void removeRangeSingle(TRangeData rangeBegin, TRangeData rangeEnd)
	{
		InternalRange range(rangeBegin, rangeEnd);
		auto itr = m_map.find(range);
		cemu_assert_debug(itr != m_map.cend());
		if (itr == m_map.cend())
			return;
		cemu_assert_debug((*itr).first.rangeBegin == rangeBegin && (*itr).first.rangeEnd == rangeEnd);
		// delete entire range
		TNodeObject* t = (*itr).second;
		m_map.erase(itr);
		TNodeObject::Delete(t);
	}

	// remove existing range that matches given begin and end without calling delete callback
	void removeRangeSingleWithoutCallback(TRangeData rangeBegin, TRangeData rangeEnd)
	{
		InternalRange range(rangeBegin, rangeEnd);
		auto itr = m_map.find(range);
		cemu_assert_debug(itr != m_map.cend());
		if (itr == m_map.cend())
			return;
		cemu_assert_debug((*itr).first.rangeBegin == rangeBegin && (*itr).first.rangeEnd == rangeEnd);
		// delete entire range
		TNodeObject* t = (*itr).second;
		m_map.erase(itr);
	}

	void splitRange(TRangeData rangeOffset)
	{
		// not well tested
		removeRange(rangeOffset, rangeOffset+1);
	}

	template<typename TFunc>
	void forEachOverlapping(TRangeData rangeBegin, TRangeData rangeEnd, TFunc f)
	{
		InternalRange range(rangeBegin, rangeEnd);
		auto itr = m_map.find(range);
		if (itr == m_map.cend())
			return;
		cemu_assert_debug(itr == m_map.lower_bound(range));
		while (itr != m_map.cend() && (*itr).first.rangeBegin < rangeEnd)
		{
			f((*itr).second, rangeBegin, rangeEnd);
			++itr;
		}
	}

	void validate()
	{
		if (m_map.empty())
			return;
		auto itr = m_map.begin();
		if ((*itr).first.rangeBegin > (*itr).first.rangeEnd)
			assert_dbg();
		TRangeData currentLoc = (*itr).first.rangeEnd;
		++itr;
		while (itr != m_map.end())
		{
			if ((*itr).first.rangeBegin >= (*itr).first.rangeEnd)
				assert_dbg(); // negative or zero size ranges are not allowed
			if (currentLoc > (*itr).first.rangeBegin)
				assert_dbg(); // stored ranges must not overlap
			currentLoc = (*itr).first.rangeEnd;
			++itr;
		}
	}

    bool empty() const
    {
        return m_map.empty();
    }

	const std::map<InternalRange, TNodeObject*>& getAll() const { return m_map; };
};

std::unique_ptr<VHeap> g_gpuBufferHeap = nullptr;
std::vector<uint8> s_pageUploadBuffer;
std::vector<class BufferCacheNode*> s_allCacheNodes;

void LatteBufferCache_removeSingleNodeFromTree(BufferCacheNode* node);

class BufferCacheNode
{
	static inline constexpr uint64 c_streamoutSig0 = 0xF0F0F0F0155C5B6Aull;
	static inline constexpr uint64 c_streamoutSig1 = 0x8BE6336411814F4Full;

public:
	// returns false if not enough space is available
	bool allocateCacheMemory()
	{
		cemu_assert_debug(m_hasCacheAlloc == false);
		cemu_assert_debug(m_rangeEnd > m_rangeBegin);
		m_hasCacheAlloc = g_gpuBufferHeap->allocOffset(m_rangeEnd - m_rangeBegin, CACHE_PAGE_SIZE, m_cacheOffset);
		return m_hasCacheAlloc;
	}

	void ReleaseCacheMemoryImmediately()
	{
		if (m_hasCacheAlloc)
		{
			cemu_assert_debug(isInUse() == false);
			g_gpuBufferHeap->freeOffset(m_cacheOffset);
			m_hasCacheAlloc = false;
		}
	}

	uint32 getBufferOffset(MPTR physAddr) const
	{
		cemu_assert_debug(m_hasCacheAlloc);
		cemu_assert_debug(physAddr >= m_rangeBegin);
		cemu_assert_debug(physAddr < m_rangeEnd);
		uint32 relOffset = physAddr - m_rangeBegin;
		return m_cacheOffset + relOffset;
	}

	void writeStreamout(MPTR rangeBegin, MPTR rangeEnd)
	{
		if ((rangeBegin & 0xF))
		{
			cemuLog_logDebug(LogType::Force, "writeStreamout(): RangeBegin not aligned to 16. Begin {:08x} End {:08x}", rangeBegin, rangeEnd);
			rangeBegin = (rangeBegin + 0xF) & ~0xF;
			rangeEnd = std::max(rangeBegin, rangeEnd);
		}
		if (rangeEnd & 0xF)
		{
			// todo - add support for 4 byte granularity for streamout writes and cache
			// used by Affordable Space Adventures and YWW Level 1-8
			// also used by CoD Ghosts (8 byte granularity)
			//cemuLog_logDebug(LogType::Force, "Streamout write size is not aligned to 16 bytes");
			rangeEnd &= ~0xF;
		}
		//cemu_assert_debug((rangeEnd & 0xF) == 0);
		rangeBegin = std::max(rangeBegin, m_rangeBegin);
		rangeEnd = std::min(rangeEnd, m_rangeEnd);
		if (rangeBegin >= rangeEnd)
			return;
		sint32 numPages = getPageCountFromRange(rangeBegin, rangeEnd);
		sint32 pageIndex = getPageIndexFromAddr(rangeBegin);

		cemu_assert_debug((m_rangeBegin + pageIndex * CACHE_PAGE_SIZE) <= rangeBegin);
		cemu_assert_debug((m_rangeBegin + (pageIndex + numPages) * CACHE_PAGE_SIZE) >= rangeEnd);

		for (sint32 i = 0; i < numPages; i++)
		{
			pageWriteStreamoutSignatures(pageIndex, rangeBegin, rangeEnd);
			pageIndex++;
			//pageInfo->hasStreamoutData = true;
			//pageInfo++;
		}
		if (numPages > 0)
			m_hasStreamoutData = true;
	}

	void checkAndSyncModifications(MPTR rangeBegin, MPTR rangeEnd, bool uploadData)
	{
		cemu_assert_debug(rangeBegin >= m_rangeBegin);
		cemu_assert_debug(rangeEnd <= m_rangeEnd);
		cemu_assert_debug(rangeBegin < m_rangeEnd);
		cemu_assert_debug((rangeBegin % CACHE_PAGE_SIZE) == 0);
		cemu_assert_debug((rangeEnd % CACHE_PAGE_SIZE) == 0);

		sint32 basePageIndex = getPageIndexFromAddrAligned(rangeBegin);
		sint32 numPages = getPageCountFromRangeAligned(rangeBegin, rangeEnd);
		uint8* pagePtr = memory_getPointerFromPhysicalOffset(rangeBegin);
		sint32 uploadPageBegin = -1;
		CachePageInfo* pageInfo = m_pageInfo.data() + basePageIndex;
		for (sint32 i = 0; i < numPages; i++)
		{
			if (pageInfo->hasStreamoutData)
			{
				// first upload any pending sequence of pages
				if (uploadPageBegin != -1)
				{
					// upload range
					if (uploadData)
						uploadPages(uploadPageBegin, basePageIndex + i);
					uploadPageBegin = -1;
				}
				// check if hash changed
				uint64 pageHash = hashPage(pagePtr);
				if (pageInfo->hash != pageHash)
				{
					pageInfo->hash = pageHash;
					// for pages that contain streamout data we do uploads with a much smaller granularity
					// and skip uploading any data that is marked with streamout filler bytes
					if (!uploadPageWithStreamoutFiltered(basePageIndex + i))
						pageInfo->hasStreamoutData = false; // all streamout data was replaced
				}
				pagePtr += CACHE_PAGE_SIZE;
				pageInfo++;
				continue;
			}

			uint64 pageHash = hashPage(pagePtr);
			pagePtr += CACHE_PAGE_SIZE;
			if (pageInfo->hash != pageHash)
			{
				if (uploadPageBegin == -1)
					uploadPageBegin = i + basePageIndex;
				pageInfo->hash = pageHash;
			}
			else
			{
				if (uploadPageBegin != -1)
				{
					// upload range
					if (uploadData)
						uploadPages(uploadPageBegin, basePageIndex + i);
					uploadPageBegin = -1;
				}
			}
			pageInfo++;
		}
		if (uploadPageBegin != -1)
		{
			if (uploadData)
				uploadPages(uploadPageBegin, basePageIndex + numPages);
		}
	}

	void checkAndSyncModifications(bool uploadData)
	{
		checkAndSyncModifications(m_rangeBegin, m_rangeEnd, uploadData);
		m_lastModifyCheckCronon = g_currentCacheChronon;
		m_hasInvalidation = false;
	}

	void checkAndSyncModificationsIfChrononChanged(MPTR reservePhysAddress, uint32 reserveSize)
	{
		if (m_lastModifyCheckCronon != g_currentCacheChronon)
		{
			m_lastModifyCheckCronon = g_currentCacheChronon;
			checkAndSyncModifications(m_rangeBegin, m_rangeEnd, true);
			m_hasInvalidation = false;
		}
		if (m_hasInvalidation)
		{
			// ideally we would only upload the pages that intersect both the reserve range and the invalidation range
			// but this would require complex per-page tracking of invalidation. Since this is on a hot path we do a cheap approximation
			// where we only track one continous invalidation range

			// try to bound uploads to the reserve range within the invalidation
			uint32 resRangeBegin = reservePhysAddress & ~CACHE_PAGE_SIZE_M1;
			uint32 resRangeEnd = ((reservePhysAddress + reserveSize) + CACHE_PAGE_SIZE_M1) & ~CACHE_PAGE_SIZE_M1;

			uint32 uploadBegin = std::max(m_invalidationRangeBegin, resRangeBegin);
			uint32 uploadEnd = std::min(resRangeEnd, m_invalidationRangeEnd);

			if (uploadBegin >= uploadEnd)
				return; // reserve range not within invalidation or range is zero sized

			
			if (uploadBegin == m_invalidationRangeBegin)
			{
				m_invalidationRangeBegin = uploadEnd;
				checkAndSyncModifications(uploadBegin, uploadEnd, true);
			}
			if (uploadEnd == m_invalidationRangeEnd)
			{
				m_invalidationRangeEnd = uploadBegin;
				checkAndSyncModifications(uploadBegin, uploadEnd, true);
			}
			else
			{
				// upload all of invalidation
				checkAndSyncModifications(m_invalidationRangeBegin, m_invalidationRangeEnd, true);
				m_invalidationRangeBegin = m_invalidationRangeEnd;
			}
			if(m_invalidationRangeEnd <= m_invalidationRangeBegin)
				m_hasInvalidation = false;
		}
	}

	void invalidate(MPTR rangeBegin, MPTR rangeEnd)
	{
		rangeBegin = std::max(rangeBegin, m_rangeBegin);
		rangeEnd = std::min(rangeEnd, m_rangeEnd);
		if (rangeBegin >= rangeEnd)
			return;
		if (m_hasInvalidation)
		{
			m_invalidationRangeBegin = std::min(m_invalidationRangeBegin, rangeBegin);
			m_invalidationRangeEnd = std::max(m_invalidationRangeEnd, rangeEnd);
		}
		else
		{
			m_invalidationRangeBegin = rangeBegin;
			m_invalidationRangeEnd = rangeEnd;
			m_hasInvalidation = true;
		}
		cemu_assert_debug(m_invalidationRangeBegin >= m_rangeBegin);
		cemu_assert_debug(m_invalidationRangeEnd <= m_rangeEnd);
		cemu_assert_debug(m_invalidationRangeBegin < m_invalidationRangeEnd);
		m_invalidationRangeBegin = m_invalidationRangeBegin & ~CACHE_PAGE_SIZE_M1;
		m_invalidationRangeEnd = (m_invalidationRangeEnd + CACHE_PAGE_SIZE_M1) & ~CACHE_PAGE_SIZE_M1;
	}

	void flagInUse()
	{
		m_lastDrawcall = LatteGPUState.drawCallCounter;
		m_lastFrame = LatteGPUState.frameCounter;
	}

	bool isInUse() const
	{
		return m_lastDrawcall == LatteGPUState.drawCallCounter;
	}

	// returns true if the range does not contain any GPU-cache-only data and can be fully restored from RAM
	bool isRAMOnly() const
	{
		return !m_hasStreamoutData;
	}

	MPTR GetRangeBegin() const { return m_rangeBegin; }
	MPTR GetRangeEnd() const { return m_rangeEnd; }

	uint32 GetDrawcallAge() const { return LatteGPUState.drawCallCounter - m_lastDrawcall; };
	uint32 GetFrameAge() const { return LatteGPUState.frameCounter - m_lastFrame; };

	bool HasStreamoutData() const { return m_hasStreamoutData; };

private:
	struct CachePageInfo
	{
		uint64 hash{ 0 };
		bool hasStreamoutData{ false };
	};

	MPTR m_rangeBegin;
	MPTR m_rangeEnd; // (exclusive)
	bool m_hasCacheAlloc{ false };
	uint32 m_cacheOffset{ 0 };
	// usage
	uint32 m_lastDrawcall;
	uint32 m_lastFrame;
	uint32 m_arrayIndex;
	// state tracking
	uint32 m_lastModifyCheckCronon{ g_currentCacheChronon - 1 };
	std::vector<CachePageInfo> m_pageInfo;
	bool m_hasStreamoutData{ false };
	// invalidation
	bool m_hasInvalidation{false};
	MPTR m_invalidationRangeBegin;
	MPTR m_invalidationRangeEnd;

	BufferCacheNode(MPTR rangeBegin, MPTR rangeEnd): m_rangeBegin(rangeBegin), m_rangeEnd(rangeEnd) 
	{
		flagInUse();
		cemu_assert_debug(rangeBegin < rangeEnd);
		size_t numPages = getPageCountFromRangeAligned(rangeBegin, rangeEnd);
		m_pageInfo.resize(numPages);
		// append to array
		m_arrayIndex = (uint32)s_allCacheNodes.size();
		s_allCacheNodes.emplace_back(this);
	};

	~BufferCacheNode()
	{
		if (m_hasCacheAlloc)
			g_deallocateQueue.emplace_back(m_cacheOffset); // release after current drawcall
		// remove from array
		auto temp = s_allCacheNodes.back();
		s_allCacheNodes.pop_back();
		if (this != temp)
		{
			s_allCacheNodes[m_arrayIndex] = temp;
			temp->m_arrayIndex = m_arrayIndex;
		}
	}

	uint32 getPageIndexFromAddrAligned(uint32 offset) const
	{
		cemu_assert_debug((offset % CACHE_PAGE_SIZE) == 0);
		return (offset - m_rangeBegin) / CACHE_PAGE_SIZE;
	}

	uint32 getPageIndexFromAddr(uint32 offset) const
	{
		offset &= ~CACHE_PAGE_SIZE_M1;
		return (offset - m_rangeBegin) / CACHE_PAGE_SIZE;
	}

	uint32 getPageCountFromRangeAligned(MPTR rangeBegin, MPTR rangeEnd) const
	{
		cemu_assert_debug((rangeBegin % CACHE_PAGE_SIZE) == 0);
		cemu_assert_debug((rangeEnd % CACHE_PAGE_SIZE) == 0);
		cemu_assert_debug(rangeBegin <= rangeEnd);
		return (rangeEnd - rangeBegin) / CACHE_PAGE_SIZE;
	}

	uint32 getPageCountFromRange(MPTR rangeBegin, MPTR rangeEnd) const
	{
		rangeEnd = (rangeEnd + CACHE_PAGE_SIZE_M1) & ~CACHE_PAGE_SIZE_M1;
		rangeBegin &= ~CACHE_PAGE_SIZE_M1;
		cemu_assert_debug(rangeBegin <= rangeEnd);
		return (rangeEnd - rangeBegin) / CACHE_PAGE_SIZE;
	}

	void syncFromRAM(MPTR rangeBegin, MPTR rangeEnd)
	{
		cemu_assert_debug(rangeBegin >= m_rangeBegin);
		cemu_assert_debug(rangeEnd <= m_rangeEnd);
		cemu_assert_debug(rangeEnd > rangeBegin);
		cemu_assert_debug(m_hasCacheAlloc);

		// reset write tracking
		checkAndSyncModifications(rangeBegin, rangeEnd, false);

		g_renderer->bufferCache_upload(memory_getPointerFromPhysicalOffset(rangeBegin), rangeEnd - rangeBegin, getBufferOffset(rangeBegin));
	}

	void syncFromNode(BufferCacheNode* srcNode)
	{
		// get shared range
		MPTR rangeBegin = std::max(m_rangeBegin, srcNode->m_rangeBegin);
		MPTR rangeEnd = std::min(m_rangeEnd, srcNode->m_rangeEnd);
		cemu_assert_debug(rangeBegin < rangeEnd);
		g_renderer->bufferCache_copy(srcNode->getBufferOffset(rangeBegin), this->getBufferOffset(rangeBegin), rangeEnd - rangeBegin);
		// copy page checksums and information
		sint32 numPages = getPageCountFromRangeAligned(rangeBegin, rangeEnd);
		CachePageInfo* pageInfoDst = this->m_pageInfo.data() + this->getPageIndexFromAddrAligned(rangeBegin);
		CachePageInfo* pageInfoSrc = srcNode->m_pageInfo.data() + srcNode->getPageIndexFromAddrAligned(rangeBegin);
		for (sint32 i = 0; i < numPages; i++)
		{
			pageInfoDst[i] = pageInfoSrc[i];
			if (pageInfoSrc[i].hasStreamoutData)
				m_hasStreamoutData = true;
		}
	}

	void uploadPages(uint32 firstPage, uint32 lastPagePlusOne)
	{
		cemu_assert_debug(lastPagePlusOne > firstPage);
		uint32 uploadRangeBegin = m_rangeBegin + firstPage * CACHE_PAGE_SIZE;
		uint32 uploadRangeEnd = m_rangeBegin + lastPagePlusOne * CACHE_PAGE_SIZE;
		cemu_assert_debug(uploadRangeEnd > uploadRangeBegin);
		// make sure uploaded pages and hashes match
		uint32 numPages = lastPagePlusOne - firstPage;
		if (s_pageUploadBuffer.size() < (numPages * CACHE_PAGE_SIZE))
			s_pageUploadBuffer.resize(numPages * CACHE_PAGE_SIZE);
		// todo - improve performance by merging memcpy + hashPage() ?
		memcpy(s_pageUploadBuffer.data(), memory_getPointerFromPhysicalOffset(uploadRangeBegin), numPages * CACHE_PAGE_SIZE);
		for (uint32 i = 0; i < numPages; i++)
		{
			m_pageInfo[firstPage + i].hash = hashPage(s_pageUploadBuffer.data() + i * CACHE_PAGE_SIZE);
		}
		g_renderer->bufferCache_upload(s_pageUploadBuffer.data(), uploadRangeEnd - uploadRangeBegin, getBufferOffset(uploadRangeBegin));
	}

	// upload only non-streamout data of a single page
	// returns true if at least one streamout 16-byte block is present
	// also updates the page hash to match the uploaded data (strict match)
	sint32 uploadPageWithStreamoutFiltered(uint32 pageIndex)
	{
		uint8 pageCopy[CACHE_PAGE_SIZE];
		memcpy(pageCopy, memory_getPointerFromPhysicalOffset(m_rangeBegin + pageIndex * CACHE_PAGE_SIZE), CACHE_PAGE_SIZE);

		MPTR pageBase = m_rangeBegin + pageIndex * CACHE_PAGE_SIZE;

		sint32 blockBegin = -1;
		uint64* pagePtrU64 = (uint64*)pageCopy;
		m_pageInfo[pageIndex].hash = hashPage(pageCopy);
		bool hasStreamoutBlocks = false;
		for (sint32 i = 0; i < CACHE_PAGE_SIZE / 16; i++)
		{
			if (pagePtrU64[0] == c_streamoutSig0 && pagePtrU64[1] == c_streamoutSig1)
			{
				hasStreamoutBlocks = true;
				if (blockBegin != -1)
				{
					uint32 uploadRelRangeBegin = blockBegin * 16;
					uint32 uploadRelRangeEnd = i * 16;
					cemu_assert_debug(uploadRelRangeEnd > uploadRelRangeBegin);
					g_renderer->bufferCache_upload(pageCopy + uploadRelRangeBegin, uploadRelRangeEnd - uploadRelRangeBegin, getBufferOffset(pageBase + uploadRelRangeBegin));
					blockBegin = -1;
				}
				pagePtrU64 += 2;
				continue;
			}
			else if (blockBegin == -1)
				blockBegin = i;
			pagePtrU64 += 2;
		}
		if (blockBegin != -1)
		{
			uint32 uploadRelRangeBegin = blockBegin * 16;
			uint32 uploadRelRangeEnd = CACHE_PAGE_SIZE;
			cemu_assert_debug(uploadRelRangeEnd > uploadRelRangeBegin);
			g_renderer->bufferCache_upload(pageCopy + uploadRelRangeBegin, uploadRelRangeEnd - uploadRelRangeBegin, getBufferOffset(pageBase + uploadRelRangeBegin));
			blockBegin = -1;
		}
		return hasStreamoutBlocks;
	}

	void shrink(MPTR newRangeBegin, MPTR newRangeEnd)
	{
		cemu_assert_debug(newRangeBegin >= m_rangeBegin);
		cemu_assert_debug(newRangeEnd >= m_rangeEnd);
		cemu_assert_debug(newRangeEnd > m_rangeBegin);
		assert_dbg(); // todo (resize page array)
		m_rangeBegin = newRangeBegin;
		m_rangeEnd = newRangeEnd;
	}

	static uint64 hashPage(uint8* mem)
	{
		static const uint64 k0 = 0x55F23EAD;
		static const uint64 k1 = 0x185FDC6D;
		static const uint64 k2 = 0xF7431F49;
		static const uint64 k3 = 0xA4C7AE9D;

		cemu_assert_debug((CACHE_PAGE_SIZE % 32) == 0);
		const uint64* ptr = (const uint64*)mem;
		const uint64* end = ptr + (CACHE_PAGE_SIZE / sizeof(uint64));

		uint64 h0 = 0;
		uint64 h1 = 0;
		uint64 h2 = 0;
		uint64 h3 = 0;
		while (ptr < end)
		{
			h0 = std::rotr(h0, 7);
			h1 = std::rotr(h1, 7);
			h2 = std::rotr(h2, 7);
			h3 = std::rotr(h3, 7);

			h0 += ptr[0] * k0;
			h1 += ptr[1] * k1;
			h2 += ptr[2] * k2;
			h3 += ptr[3] * k3;
			ptr += 4;
		}

		return h0 + h1 + h2 + h3;
	}

	// flag page as having streamout data, also write streamout signatures to page memory
	// also incrementally updates the page hash to include the written signatures, this prevents signature writes from triggering a data upload
	void pageWriteStreamoutSignatures(uint32 pageIndex, MPTR rangeBegin, MPTR rangeEnd)
	{
		uint32 pageRangeBegin = m_rangeBegin + pageIndex * CACHE_PAGE_SIZE;
		uint32 pageRangeEnd = pageRangeBegin + CACHE_PAGE_SIZE;
		rangeBegin = std::max(pageRangeBegin, rangeBegin);
		rangeEnd = std::min(pageRangeEnd, rangeEnd);
		cemu_assert_debug(rangeEnd > rangeBegin);
		cemu_assert_debug(rangeBegin >= pageRangeBegin);
		cemu_assert_debug(rangeEnd <= pageRangeEnd);
		cemu_assert_debug((rangeBegin & 0xF) == 0);
		cemu_assert_debug((rangeEnd & 0xF) == 0);
		
		auto pageInfo = m_pageInfo.data() + pageIndex;
		pageInfo->hasStreamoutData = true;

		// if the whole page is replaced we can use a cached hash
		if (pageRangeBegin == rangeBegin && pageRangeEnd == rangeEnd)
		{
			uint64* pageMem = (uint64*)memory_getPointerFromPhysicalOffset(rangeBegin);
			uint32 numBlocks = (rangeEnd - rangeBegin) / 16;
			for (uint32 i = 0; i < numBlocks; i++)
			{
				pageMem[0] = c_streamoutSig0;
				pageMem[1] = c_streamoutSig1;
				pageMem += 2;
			}

			pageInfo->hash = c_fullStreamoutPageHash;
			return;
		}

		uint64* pageMem = (uint64*)memory_getPointerFromPhysicalOffset(rangeBegin);
		uint32 numBlocks = (rangeEnd - rangeBegin) / 16;
		uint32 indexHashBlock = (rangeBegin - pageRangeBegin) / sizeof(uint64);
		for (uint32 i = 0; i < numBlocks; i++)
		{
			pageMem[0] = c_streamoutSig0;
			pageMem[1] = c_streamoutSig1;
			pageMem += 2;
		}
		pageInfo->hash = 0; // reset hash
	}

	static uint64 genStreamoutPageHash()
	{
		uint8 pageMem[CACHE_PAGE_SIZE];
		uint64* pageMemU64 = (uint64*)pageMem;
		for (uint32 i = 0; i < sizeof(pageMem) / sizeof(uint64) / 2; i++)
		{
			pageMemU64[0] = c_streamoutSig0;
			pageMemU64[1] = c_streamoutSig1;
			pageMemU64 += 2;
		}

		return hashPage(pageMem);
	}

	static inline uint64 c_fullStreamoutPageHash = genStreamoutPageHash();
	static std::vector<uint32> g_deallocateQueue;

public:
    static void UnloadAll()
    {
        size_t i = 0;
        while (i < s_allCacheNodes.size())
        {
            BufferCacheNode* node = s_allCacheNodes[i];
            node->ReleaseCacheMemoryImmediately();
            LatteBufferCache_removeSingleNodeFromTree(node);
            delete node;
        }
        for(auto& it : s_allCacheNodes)
            delete it;
        s_allCacheNodes.clear();
        g_deallocateQueue.clear();
    }
	
	static void ProcessDeallocations()
	{
		for(auto& itr : g_deallocateQueue)
			g_gpuBufferHeap->freeOffset(itr);
		g_deallocateQueue.clear();
	}

	// drops everything from the cache that isn't considered in use or unrestorable (ranges with streamout)
	static void CleanupCacheAggressive(MPTR excludedRangeBegin, MPTR excludedRangeEnd)
	{
		size_t i = 0;
		while (i < s_allCacheNodes.size())
		{
			BufferCacheNode* node = s_allCacheNodes[i];
			if (node->isInUse())
			{
				i++;
				continue;
			}
			if(!node->isRAMOnly())
			{
				i++;
				continue;
			}
			if(node->GetRangeBegin() < excludedRangeEnd && node->GetRangeEnd() > excludedRangeBegin)
			{
				i++;
				continue;
			}
			// delete range
			node->ReleaseCacheMemoryImmediately();
			LatteBufferCache_removeSingleNodeFromTree(node);
			delete node;
		}
	}

	/* callbacks from IntervalTree */

	static BufferCacheNode* Create(MPTR rangeBegin, MPTR rangeEnd, std::span<BufferCacheNode*> overlappingObjects)
	{
		auto newRange = new BufferCacheNode(rangeBegin, rangeEnd);
		if (!newRange->allocateCacheMemory())
		{
			// not enough memory available, try to drop ram-only ranges from the ones we replace
			for (size_t i = 0; i < overlappingObjects.size(); i++)
			{
				BufferCacheNode* nodeItr = overlappingObjects[i];
				if (!nodeItr->isInUse() && nodeItr->isRAMOnly())
				{
					nodeItr->ReleaseCacheMemoryImmediately();
					delete nodeItr;
					overlappingObjects[i] = nullptr;
				}
			}
			// retry allocation
			if (!newRange->allocateCacheMemory())
			{
				cemuLog_log(LogType::Force, "Out-of-memory in GPU buffer (trying to allocate: {}KB) Cleaning up cache...", (rangeEnd - rangeBegin + 1023) / 1024);
				CleanupCacheAggressive(rangeBegin, rangeEnd);
				if (!newRange->allocateCacheMemory())
				{
					cemuLog_log(LogType::Force, "Failed to free enough memory in GPU buffer");
					cemu_assert(false);
				}
			}
		}
		newRange->syncFromRAM(rangeBegin, rangeEnd); // possible small optimization: only load the ranges from RAM which are not overwritten by ->syncFromNode()
		for (auto itr : overlappingObjects)
		{
			if(itr == nullptr)
				continue;
			newRange->syncFromNode(itr);
			delete itr;
		}
		return newRange;
	}

	static void Delete(BufferCacheNode* nodeObject)
	{
		delete nodeObject;
	}

	static void Resize(BufferCacheNode* nodeObject, MPTR rangeBegin, MPTR rangeEnd)
	{
		nodeObject->shrink(rangeBegin, rangeEnd);
	}

	static BufferCacheNode* Split(BufferCacheNode* nodeObject, MPTR firstRangeBegin, MPTR firstRangeEnd, MPTR secondRangeBegin, MPTR secondRangeEnd)
	{
		auto newRange = new BufferCacheNode(secondRangeBegin, secondRangeEnd);
		// todo - add support for splitting BufferCacheNode memory allocations, then we dont need to do a separate allocation
		if (!newRange->allocateCacheMemory())
		{
			cemuLog_log(LogType::Force, "Out-of-memory in GPU buffer during split operation");
			cemu_assert(false);
		}
		newRange->syncFromNode(nodeObject);
		nodeObject->shrink(firstRangeBegin, firstRangeEnd);
		return newRange;
	}
};

std::vector<uint32> BufferCacheNode::g_deallocateQueue;
IntervalTree2<MPTR, BufferCacheNode> g_gpuBufferCache;

void LatteBufferCache_removeSingleNodeFromTree(BufferCacheNode* node)
{
	g_gpuBufferCache.removeRangeSingleWithoutCallback(node->GetRangeBegin(), node->GetRangeEnd());
}

BufferCacheNode* LatteBufferCache_reserveRange(MPTR physAddress, uint32 size)
{
	MPTR rangeStart = physAddress - (physAddress % CACHE_PAGE_SIZE);
	MPTR rangeEnd = (physAddress + size + CACHE_PAGE_SIZE_M1) & ~CACHE_PAGE_SIZE_M1;

	auto range = g_gpuBufferCache.getRange(rangeStart, rangeEnd);
	if (!range)
	{
		g_gpuBufferCache.addRange(rangeStart, rangeEnd);
		range = g_gpuBufferCache.getRange(rangeStart, rangeEnd);
		cemu_assert_debug(range);
	}
	cemu_assert_debug(range->GetRangeBegin() <= physAddress);
	cemu_assert_debug(range->GetRangeEnd() >= (physAddress + size));
	return range;
}


uint32 LatteBufferCache_retrieveDataInCache(MPTR physAddress, uint32 size)
{
	auto range = LatteBufferCache_reserveRange(physAddress, size);
	range->flagInUse();

	range->checkAndSyncModificationsIfChrononChanged(physAddress, size);

	return range->getBufferOffset(physAddress);
}

void LatteBufferCache_copyStreamoutDataToCache(MPTR physAddress, uint32 size, uint32 streamoutBufferOffset)
{
	if (size == 0)
		return;
	cemu_assert_debug(size >= 16);

	auto range = LatteBufferCache_reserveRange(physAddress, size);
	range->flagInUse();

	g_renderer->bufferCache_copyStreamoutToMainBuffer(streamoutBufferOffset, range->getBufferOffset(physAddress), size);

	// write streamout signatures, flag affected pages
	range->writeStreamout(physAddress, (physAddress + size));
}

void LatteBufferCache_invalidate(MPTR physAddress, uint32 size)
{
	if (size == 0)
		return;
	g_gpuBufferCache.forEachOverlapping(physAddress, physAddress + size, [](BufferCacheNode* node, MPTR invalidationRangeBegin, MPTR invalidationRangeEnd)
		{
			node->invalidate(invalidationRangeBegin, invalidationRangeEnd);
		}
	);
}

// optimized version of LatteBufferCache_invalidate() if physAddress points to the beginning of a page
void LatteBufferCache_invalidatePage(MPTR physAddress)
{
	cemu_assert_debug((physAddress & CACHE_PAGE_SIZE_M1) == 0);
	BufferCacheNode* node = g_gpuBufferCache.getRangeByPoint(physAddress);
	if (node)
		node->invalidate(physAddress, physAddress+CACHE_PAGE_SIZE);
}

void LatteBufferCache_processDeallocations()
{
	BufferCacheNode::ProcessDeallocations();
}

void LatteBufferCache_init(size_t bufferSize)
{
    cemu_assert_debug(g_gpuBufferCache.empty());
	g_gpuBufferHeap.reset(new VHeap(nullptr, (uint32)bufferSize));
	g_renderer->bufferCache_init((uint32)bufferSize);
}

void LatteBufferCache_UnloadAll()
{
    BufferCacheNode::UnloadAll();
}

void LatteBufferCache_getStats(uint32& heapSize, uint32& allocationSize, uint32& allocNum)
{
	g_gpuBufferHeap->getStats(heapSize, allocationSize, allocNum);
}

FSpinlock g_spinlockDCFlushQueue;

class SparseBitset
{
	static inline constexpr size_t TABLE_MASK = 0xFF;

public:
	bool Empty() const
	{
		return m_numNonEmptyVectors == 0;
	}

	void Set(uint32 index)
	{
		auto& v = m_bits[index & TABLE_MASK];
		if (std::find(v.cbegin(), v.cend(), index) != v.end())
			return;
		if (v.empty())
		{
			m_nonEmptyVectors[m_numNonEmptyVectors] = &v;
			m_numNonEmptyVectors++;
		}
		v.emplace_back(index);
	}

	template<typename TFunc>
	void ForAllAndClear(TFunc callbackFunc)
	{
		auto vCurrent = m_nonEmptyVectors + 0;
		auto vEnd = m_nonEmptyVectors + m_numNonEmptyVectors;
		while (vCurrent < vEnd)
		{
			std::vector<uint32>* vec = *vCurrent;
			vCurrent++;
			for (const auto& it : *vec)
				callbackFunc(it);
			vec->clear();
		}
		m_numNonEmptyVectors = 0;
	}

	void Clear()
	{
		auto vCurrent = m_nonEmptyVectors + 0;
		auto vEnd = m_nonEmptyVectors + m_numNonEmptyVectors;
		while (vCurrent < vEnd)
		{
			std::vector<uint32>* vec = *vCurrent;
			vCurrent++;
			vec->clear();
		}
		m_numNonEmptyVectors = 0;
	}

private:
	std::vector<uint32> m_bits[TABLE_MASK + 1];
	std::vector<uint32>* m_nonEmptyVectors[TABLE_MASK + 1];
	size_t m_numNonEmptyVectors{ 0 };
};

SparseBitset* s_DCFlushQueue = new SparseBitset();
SparseBitset* s_DCFlushQueueAlternate = new SparseBitset();

void LatteBufferCache_notifyDCFlush(MPTR address, uint32 size)
{
	if (address == 0 || size == 0xFFFFFFFF)
		return; // global flushes are ignored for now

	uint32 firstPage = address / CACHE_PAGE_SIZE;
	uint32 lastPage = (address + size - 1) / CACHE_PAGE_SIZE;
	g_spinlockDCFlushQueue.lock();
	for (uint32 i = firstPage; i <= lastPage; i++)
		s_DCFlushQueue->Set(i);
	g_spinlockDCFlushQueue.unlock();
}

void LatteBufferCache_processDCFlushQueue()
{
	if (s_DCFlushQueue->Empty()) // quick check to avoid locking if there is no work to do
		return;
	g_spinlockDCFlushQueue.lock();
	std::swap(s_DCFlushQueue, s_DCFlushQueueAlternate);
	g_spinlockDCFlushQueue.unlock();
	s_DCFlushQueueAlternate->ForAllAndClear([](uint32 index) {LatteBufferCache_invalidatePage(index * CACHE_PAGE_SIZE); });
}

void LatteBufferCache_notifyDrawDone()
{

}

void LatteBufferCache_notifySwapTVScanBuffer()
{
	if( ActiveSettings::FlushGPUCacheOnSwap() )
		g_currentCacheChronon++;
}

void LatteBufferCache_incrementalCleanup()
{
	static uint32 s_counter = 0;

	if (s_allCacheNodes.empty())
		return;

	s_counter++;
	s_counter %= (uint32)s_allCacheNodes.size();

	auto range = s_allCacheNodes[s_counter];

	if (range->HasStreamoutData())
	{
		// currently we never delete streamout ranges
		// todo - check if streamout pages got overwritten + if the range would lose the hasStreamoutData flag
		return;
	}

	uint32 heapSize;
	uint32 allocationSize;
	uint32 allocNum;
	g_gpuBufferHeap->getStats(heapSize, allocationSize, allocNum);

	if (allocationSize >= (heapSize * 4 / 5))
	{
		// heap is 80% filled
		if (range->GetFrameAge() >= 2)
		{
			g_gpuBufferCache.removeRangeSingle(range->GetRangeBegin(), range->GetRangeEnd());
		}
	}
	else if (allocationSize >= (heapSize * 3 / 4))
	{
		// heap is 75-100% filled
		if (range->GetFrameAge() >= 4)
		{
			g_gpuBufferCache.removeRangeSingle(range->GetRangeBegin(), range->GetRangeEnd());
		}
	}
	else if (allocationSize >= (heapSize / 2))
	{
		// if heap is 50-75% filled
		if (range->GetFrameAge() >= 20)
		{
			g_gpuBufferCache.removeRangeSingle(range->GetRangeBegin(), range->GetRangeEnd());
		}
	}
	else
	{
		// heap is under 50% capacity
		if (range->GetFrameAge() >= 500)
		{
			g_gpuBufferCache.removeRangeSingle(range->GetRangeBegin(), range->GetRangeEnd());
		}
	}
}
