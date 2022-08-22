#pragma once

void LatteBufferCache_init(size_t bufferSize);

uint32 LatteBufferCache_retrieveDataInCache(MPTR physAddress, uint32 size);
void LatteBufferCache_copyStreamoutDataToCache(MPTR physAddress, uint32 size, uint32 streamoutBufferOffset);
void LatteBufferCache_invalidate(MPTR physAddress, uint32 size);

void LatteBufferCache_notifyDCFlush(MPTR address, uint32 size);
void LatteBufferCache_processDCFlushQueue();

void LatteBufferCache_processDeallocations();
void LatteBufferCache_incrementalCleanup();

void LatteBufferCache_getStats(uint32& heapSize, uint32& allocationSize, uint32& allocNum);

void LatteBufferCache_notifySwapTVScanBuffer();