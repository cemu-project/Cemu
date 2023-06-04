#pragma once

template<typename _OBJ, typename _ADDR, size_t count, size_t granularity>
class RangeStore
{
public:

	typedef struct
	{
		_ADDR start;
		_ADDR end;
		_OBJ data;
		size_t lastIterationIndex;
	}rangeEntry_t;

	RangeStore() = default;

	size_t getBucket(_ADDR addr)
	{
		size_t index = addr / granularity;
		index %= count;
		return index;
	}

	void getBucketRange(_ADDR addrStart, _ADDR addrEnd, size_t& bucketFirst, size_t& bucketCount)
	{
		bucketFirst = getBucket(addrStart);
		size_t indexStart = addrStart / granularity;
		size_t indexEnd = std::max(addrStart, addrEnd - 1) / granularity;
		bucketCount = indexEnd - indexStart + 1;
	}

	// end address should be supplied as start+size
	void* storeRange(_OBJ data, _ADDR start, _ADDR end)
	{
		size_t bucketFirst;
		size_t bucketCount;
		getBucketRange(start, end, bucketFirst, bucketCount);
		bucketCount = std::min(bucketCount, count);

		// create range
		rangeEntry_t* rangeEntry = new rangeEntry_t();
		rangeEntry->data = data;
		rangeEntry->start = start;
		rangeEntry->end = end;
		rangeEntry->lastIterationIndex = currentIterationIndex;

		// register range in every bucket it touches
		size_t idx = bucketFirst;
		for (size_t i = 0; i < bucketCount; i++)
		{
			rangeBuckets[idx].list_ranges.push_back(rangeEntry);
			idx = (idx + 1) % count;
		}

		return rangeEntry;
	}

	void deleteRange(void* rangePtr)
	{
		rangeEntry_t* rangeEntry = (rangeEntry_t*)rangePtr;
		// get bucket range
		size_t bucketFirst;
		size_t bucketCount;
		getBucketRange(rangeEntry->start, rangeEntry->end, bucketFirst, bucketCount);
		bucketCount = std::min(bucketCount, count);
		// remove from buckets
		size_t idx = bucketFirst;
		for (size_t i = 0; i < bucketCount; i++)
		{
			rangeBuckets[idx].list_ranges.erase(std::remove(rangeBuckets[idx].list_ranges.begin(), rangeBuckets[idx].list_ranges.end(), rangeEntry), rangeBuckets[idx].list_ranges.end());
			idx = (idx + 1) % count;
		}
		delete rangeEntry;
	}

	void findRanges(_ADDR start, _ADDR end, std::function <void(_ADDR start, _ADDR end, _OBJ data)> f)
	{
		currentIterationIndex++;
		size_t bucketFirst;
		size_t bucketCount;
		getBucketRange(start, end, bucketFirst, bucketCount);
		bucketCount = std::min(bucketCount, count);
		size_t idx = bucketFirst;
		for (size_t i = 0; i < bucketCount; i++)
		{
			for (auto r : rangeBuckets[idx].list_ranges)
			{
				if (start < r->end && end > r->start && r->lastIterationIndex != currentIterationIndex)
				{
					r->lastIterationIndex = currentIterationIndex;
					f(r->start, r->end, r->data);
				}
			}
			idx = (idx + 1) % count;
		}
	}

	bool findFirstRange(_ADDR start, _ADDR end, _ADDR& rStart, _ADDR& rEnd, _OBJ& rData)
	{
		currentIterationIndex++;
		size_t bucketFirst;
		size_t bucketCount;
		getBucketRange(start, end, bucketFirst, bucketCount);
		bucketCount = std::min(bucketCount, count);
		size_t idx = bucketFirst;
		for (size_t i = 0; i < bucketCount; i++)
		{
			for (auto r : rangeBuckets[idx].list_ranges)
			{
				if (start < r->end && end > r->start && r->lastIterationIndex != currentIterationIndex)
				{
					r->lastIterationIndex = currentIterationIndex;
					rStart = r->start;
					rEnd = r->end;
					rData = r->data;
					return true;
				}
			}
			idx = (idx + 1) % count;
		}
		return false;
	}

    void clear()
    {
        for(auto& bucket : rangeBuckets)
        {
            while(!bucket.list_ranges.empty())
                deleteRange(bucket.list_ranges[0]);
        }
    }

private:
	typedef struct
	{
		std::vector<rangeEntry_t*> list_ranges;
	}rangeBucket_t;

	std::array<rangeBucket_t, count> rangeBuckets;

	size_t currentIterationIndex;
};
