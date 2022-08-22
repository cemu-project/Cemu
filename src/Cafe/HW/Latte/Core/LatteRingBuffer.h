
typedef struct
{
	uint8* data;
	sint32 size;
	sint32 writeIndex;
}LatteRingBuffer_t;

LatteRingBuffer_t* LatteRingBuffer_create(uint8* data, uint32 size);
uint8* LatteRingBuffer_allocate(LatteRingBuffer_t* rb, sint32 size, sint32 alignment);