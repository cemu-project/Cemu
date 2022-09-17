#pragma once

template<typename TData, typename TAddr, TAddr TAddressGranularity, int TBucketCount>
class IntervalBucketContainer
{
	struct bucketEntry_t
	{
		TAddr rangeStart;
		TAddr rangeEnd;
		TData* data;
		int bucketStartIndex;
		bucketEntry_t(TAddr rangeStart, TAddr rangeEnd, TData* data, int bucketStartIndex) : rangeStart(rangeStart), rangeEnd(rangeEnd), data(data), bucketStartIndex(bucketStartIndex) {};
	};
	std::vector<bucketEntry_t> list_bucket[TBucketCount];

public:

	IntervalBucketContainer() = default;

	// range is defined as inclusive rangeStart and exclusive rangeEnd
	void addRange(TAddr rangeStart, TAddr rangeEnd, TData* data)
	{
		assert(rangeStart < rangeEnd);
		int bucketStartIndex = (rangeStart / TAddressGranularity);
		int bucketEndIndex = ((rangeEnd + TAddressGranularity - 1) / TAddressGranularity);
		int bucketItrCount = bucketEndIndex - bucketStartIndex;
		bucketStartIndex %= TBucketCount;
		int bucketFirstIndex = bucketStartIndex;
		bucketItrCount = std::min(bucketItrCount, TBucketCount);
		assert(bucketItrCount != 0);
		while (bucketItrCount--)
		{
			list_bucket[bucketStartIndex].emplace_back(rangeStart, rangeEnd, data, bucketFirstIndex);
			bucketStartIndex = (bucketStartIndex + 1) % TBucketCount;
		}
	}

	void removeRange(TAddr rangeStart, TAddr rangeEnd, TData* data)
	{
		assert(rangeStart < rangeEnd);
		int bucketStartIndex = (rangeStart / TAddressGranularity);
		int bucketEndIndex = ((rangeEnd + TAddressGranularity - 1) / TAddressGranularity);
		int bucketItrCount = bucketEndIndex - bucketStartIndex;
		bucketStartIndex %= TBucketCount;
		bucketItrCount = std::min(bucketItrCount, TBucketCount);
		assert(bucketItrCount != 0);
		int eraseCountVerifier = bucketItrCount;
		while (bucketItrCount--)
		{
			for (auto it = list_bucket[bucketStartIndex].begin(); it != list_bucket[bucketStartIndex].end(); it++)
			{
				if (it->data == data)
				{
					assert(it->rangeStart == rangeStart && it->rangeEnd == rangeEnd);
					// erase
					list_bucket[bucketStartIndex].erase(it);
					eraseCountVerifier--;
					break;
				}
			}
			bucketStartIndex = (bucketStartIndex + 1) % TBucketCount;
		}
		assert(eraseCountVerifier == 0); // triggers if rangeStart/End doesn't match up with any registered range
	}

	template<typename TRangeCallback>
	void lookupRanges(TAddr rangeStart, TAddr rangeEnd, TRangeCallback cb)
	{
		assert(rangeStart < rangeEnd);
		int bucketStartIndex = (rangeStart / TAddressGranularity);
		int bucketEndIndex = ((rangeEnd + TAddressGranularity - 1) / TAddressGranularity);
		int bucketItrCount = bucketEndIndex - bucketStartIndex;
		bucketStartIndex %= TBucketCount;
		bucketItrCount = std::min(bucketItrCount, TBucketCount);
		assert(bucketItrCount != 0);
		// in first round we dont need to check if bucket was already visited
		for (auto& itr : list_bucket[bucketStartIndex])
		{
			if (itr.rangeStart < rangeEnd && itr.rangeEnd > rangeStart)
			{
				cb(itr.data);
			}
		}
		bucketItrCount--;
		bucketStartIndex = (bucketStartIndex + 1) % TBucketCount;
		// for remaining buckets check if the range starts in the current bucket
		while (bucketItrCount--)
		{
			for (auto& itr : list_bucket[bucketStartIndex])
			{
				if (itr.rangeStart < rangeEnd && itr.rangeEnd > rangeStart && itr.bucketStartIndex == bucketStartIndex)
				{
					cb(itr.data);
				}
			}
			bucketStartIndex = (bucketStartIndex + 1) % TBucketCount;
		}
	}

};