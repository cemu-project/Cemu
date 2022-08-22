// IOS
typedef struct _ioBufferVector_t ioBufferVector_t;

sint32 __depr__IOS_Open(char* path, uint32 mode);
sint32 __depr__IOS_Ioctl(uint32 fd, uint32 request, void* inBuffer, uint32 inSize, void* outBuffer, uint32 outSize);
sint32 __depr__IOS_Ioctlv(uint32 fd, uint32 request, uint32 countIn, uint32 countOut, ioBufferVector_t* ioBufferVectors);
sint32 __depr__IOS_Close(uint32 fd);

// superseded by coreinit_IPC.h