#include "Cafe/HW/Latte/Core/LatteRingBuffer.h"

LatteRingBuffer_t* LatteRingBuffer_create(uint8* data, uint32 size)
{
	LatteRingBuffer_t* rb = (LatteRingBuffer_t*)malloc(sizeof(LatteRingBuffer_t));
	rb->data = data;
	rb->size = size;
	rb->writeIndex = 0;
	return rb;
}

uint8* LatteRingBuffer_allocate(LatteRingBuffer_t* rb, sint32 size, sint32 alignment)
{
#ifdef CEMU_DEBUG_ASSERT
	cemu_assert_debug(size < rb->size);
#endif
	// align
	rb->writeIndex = (rb->writeIndex + alignment - 1)&~(alignment-1);
	// handle wrap-around
	if ((rb->writeIndex + size) >= rb->size)
		rb->writeIndex = 0;
	// allocate range
	uint8* data = rb->data + rb->writeIndex;
	rb->writeIndex += size;
	return data;
}