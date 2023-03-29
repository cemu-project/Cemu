#include "VirtualHeap.h"

VirtualBufferHeap_t* virtualBufferHeap_create(uint32 virtualHeapSize, void* baseAddr)
{
	VirtualBufferHeap_t* bufferHeap = (VirtualBufferHeap_t*)malloc(sizeof(VirtualBufferHeap_t));
	memset(bufferHeap, 0, sizeof(VirtualBufferHeap_t));
	bufferHeap->firstEntry = nullptr;
	virtualHeapSize = (virtualHeapSize + 31)&~31;
	bufferHeap->virtualSize = virtualHeapSize;
	bufferHeap->baseAddress = baseAddr;
	bufferHeap->updateTrackIndex = 0;
	// create pool of unused entries
	sint32 unusedEntryPoolSize = 1024 * 16;
	VirtualBufferHeapEntry_t* unusedEntryPool = (VirtualBufferHeapEntry_t*)malloc(sizeof(VirtualBufferHeapEntry_t)*unusedEntryPoolSize);
	for (sint32 i = 0; i < unusedEntryPoolSize - 1; i++)
	{
		unusedEntryPool[i].next = unusedEntryPool + i + 1;
	}
	unusedEntryPool[unusedEntryPoolSize - 1].next = nullptr;
	bufferHeap->firstUnusedEntry = unusedEntryPool + 0;
	return bufferHeap;
}

VirtualBufferHeapEntry_t* virtualBufferHeap_createEntry(VirtualBufferHeap_t* bufferHeap)
{
	VirtualBufferHeapEntry_t* newEntry = bufferHeap->firstUnusedEntry;
	if (newEntry == nullptr)
	{
		cemuLog_log(LogType::Force, "virtualBufferHeap_createEntry: Pool empty");
		cemu_assert_suspicious();
	}
	bufferHeap->firstUnusedEntry = newEntry->next;
	newEntry->previous = NULL;
	newEntry->next = NULL;
	return newEntry;
}

void virtualBufferHeap_releaseEntry(VirtualBufferHeap_t* bufferHeap, VirtualBufferHeapEntry_t* entry)
{
	bufferHeap->stats.allocatedMemory -= (entry->endOffset - entry->startOffset);
	bufferHeap->stats.numActiveAllocs--;
	entry->next = bufferHeap->firstUnusedEntry;
	bufferHeap->firstUnusedEntry = entry;
}

// Allocate memory region from virtual heap. Always allocates memory at the lowest possible address
VirtualBufferHeapEntry_t* virtualBufferHeap_allocate(VirtualBufferHeap_t* bufferHeap, uint32 size)
{
	// align size
	size = (size + 255)&~255;
	// iterate already allocated entries and try to find free space between them
	VirtualBufferHeapEntry_t* entryItr = bufferHeap->firstEntry;
	if (entryItr == NULL)
	{
		// entire heap is unallocated
		VirtualBufferHeapEntry_t* newEntry = virtualBufferHeap_createEntry(bufferHeap);
		newEntry->startOffset = 0;
		newEntry->endOffset = size;
		newEntry->previous = NULL;
		newEntry->next = NULL;
		bufferHeap->firstEntry = newEntry;
		bufferHeap->stats.allocatedMemory += size;
		bufferHeap->stats.numActiveAllocs++;
		return newEntry;
	}
	else
	{
		uint32 currentAllocationOffset = 0;
		VirtualBufferHeapEntry_t* entryPrev = nullptr;
		while (entryItr)
		{
			if ((currentAllocationOffset + size) > entryItr->startOffset)
			{
				// space occupied
				currentAllocationOffset = entryItr->endOffset;
				currentAllocationOffset = (currentAllocationOffset + 255)&~255;
				// next
				entryPrev = entryItr;
				entryItr = entryItr->next;
				continue;
			}
			else
			{
				if ((currentAllocationOffset + size) > bufferHeap->virtualSize)
					return nullptr; // out of heap memory
				// free space found
				VirtualBufferHeapEntry_t* newEntry = virtualBufferHeap_createEntry(bufferHeap);
				newEntry->startOffset = currentAllocationOffset;
				newEntry->endOffset = currentAllocationOffset + size;
				// insert between previous entry and entryItr
				newEntry->previous = entryItr->previous;
				newEntry->next = entryItr;
				if (entryItr->previous)
					entryItr->previous->next = newEntry;
				else
					bufferHeap->firstEntry = newEntry;
				entryItr->previous = newEntry;
				bufferHeap->stats.allocatedMemory += size;
				bufferHeap->stats.numActiveAllocs++;
				return newEntry;
			}
		}
		// add after entryPrev
		if ((currentAllocationOffset + size) > bufferHeap->virtualSize)
			return NULL; // out of heap memory
		VirtualBufferHeapEntry_t* newEntry = virtualBufferHeap_createEntry(bufferHeap);
		newEntry->startOffset = currentAllocationOffset;
		newEntry->endOffset = currentAllocationOffset + size;
		// insert after previous entry
		cemu_assert_debug(entryPrev);
		cemu_assert_debug(entryPrev->next == nullptr);
		newEntry->previous = entryPrev;
		newEntry->next = entryPrev->next;
		entryPrev->next = newEntry;
		bufferHeap->stats.allocatedMemory += size;
		bufferHeap->stats.numActiveAllocs++;
		return newEntry;
	}
	return NULL;
}

void virtualBufferHeap_free(VirtualBufferHeap_t* bufferHeap, VirtualBufferHeapEntry_t* entry)
{
	if (entry->previous == NULL)
	{
		// make the next entry the first one
		if (entry->next)
			entry->next->previous = NULL;
		bufferHeap->firstEntry = entry->next;
	}
	else
		entry->previous->next = entry->next;

	if (entry->next)
		entry->next->previous = entry->previous;
	// release entry
	virtualBufferHeap_releaseEntry(bufferHeap, entry);
}

void* virtualBufferHeap_allocateAddr(VirtualBufferHeap_t* bufferHeap, uint32 size)
{
	VirtualBufferHeapEntry_t* heapEntry = virtualBufferHeap_allocate(bufferHeap, size);
	return ((uint8*)bufferHeap->baseAddress + heapEntry->startOffset);
}

void virtualBufferHeap_freeAddr(VirtualBufferHeap_t* bufferHeap, void* addr)
{
	auto entry = bufferHeap->firstEntry;
	while(entry)
	{
		const auto entry_address = (uint8*)bufferHeap->baseAddress + entry->startOffset;
		if(entry_address == (uint8*)addr)
		{
			virtualBufferHeap_free(bufferHeap, entry);
			return;
		}
		entry = entry->next;
	}
	cemu_assert_suspicious();
}
