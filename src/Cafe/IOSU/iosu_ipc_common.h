#pragma once

using IOSDevHandle = uint32;

enum class IPCDriverState : uint32
{
	CLOSED = 1,
	INITIALIZED = 2,
	READY = 3,
	SUBMITTING = 4
};

enum class IPCCommandId : uint32
{
	IOS_OPEN = 1,
	IOS_CLOSE = 2,

	IOS_IOCTL = 6,
	IOS_IOCTLV = 7,
};

struct IPCIoctlVector
{
	MEMPTR<void> baseVirt;
	uint32be size;
	MEMPTR<void> basePhys;
};

static_assert(sizeof(IPCIoctlVector) == 0xC);

struct IPCCommandBody
{
	/* +0x00 */ betype<IPCCommandId> cmdId;
	/* +0x04 */ uint32be result; // set by IOSU
	/* +0x08 */ betype<IOSDevHandle> devHandle;
	/* +0x0C */ uint32be ukn0C;
	/* +0x10 */ uint32be ukn10;
	/* +0x14 */ uint32be ukn14;
	/* +0x18 */ uint32be ukn18;
	/* +0x1C */ uint32be ukn1C;
	/* +0x20 */ uint32be ukn20;
	/* +0x24 */ uint32be args[5];
	// anything below may only be present on the PPC side?
	/* +0x38 */ betype<IPCCommandId> prev_cmdId;
	/* +0x3C */ betype<IOSDevHandle> prev_devHandle;
	/* +0x40 */ MEMPTR<void> ppcVirt0;
	/* +0x44 */ MEMPTR<void> ppcVirt1;

	/*
		IOS_Open:
		args[0] = const char* path
		args[1] = pathLen + 1
		args[2] = flags

		IOS_Close:
		Only devHandle is set

		IOS_Ioctl:
		args[0] = requestId
		args[1] = ptrIn
		args[2] = sizeIn
		args[3] = ptrOut
		args[4] = sizeOut

		IOS_Ioctlv:
		args[0] = requestId
		args[1] = numIn
		args[2] = numOut
		args[3] = IPCIoctlVector*


	*/
};
