#pragma once

#include "iosu_ioctl.h"

// custom dev/boss protocol (Cemu only)
#define IOSU_BOSS_REQUEST_CEMU (0xEE)

typedef struct
{
	uint32 requestCode;
	// input
	uint32 accountId;
	char* taskId;
	bool bool_parameter;
	uint64 titleId;
	uint32 timeout;
	uint32 waitState;
	uint32 uk1;
	void* settings;

	// output
	uint32 returnCode; // ACP return value
	union
	{
		struct
		{
			uint32 exec_count;
			uint32 result;
		} u32;
		struct
		{
			uint32 exec_count;
			uint64 result;
		} u64;
	};
}iosuBossCemuRequest_t;

#define IOSU_NN_BOSS_TASK_RUN							(0x01)
#define IOSU_NN_BOSS_TASK_GET_CONTENT_LENGTH			(0x02)
#define IOSU_NN_BOSS_TASK_GET_PROCESSED_LENGTH			(0x03)
#define IOSU_NN_BOSS_TASK_GET_HTTP_STATUS_CODE			(0x04)
#define IOSU_NN_BOSS_TASK_GET_TURN_STATE				(0x05)
#define IOSU_NN_BOSS_TASK_WAIT							(0x06)
#define IOSU_NN_BOSS_TASK_REGISTER						(0x07)
#define IOSU_NN_BOSS_TASK_IS_REGISTERED					(0x08)
#define IOSU_NN_BOSS_TASK_REGISTER_FOR_IMMEDIATE_RUN	(0x09)
#define IOSU_NN_BOSS_TASK_UNREGISTER					(0x0A)
#define IOSU_NN_BOSS_TASK_START_SCHEDULING				(0x0B)
#define IOSU_NN_BOSS_TASK_STOP_SCHEDULING				(0x0C)

namespace iosu
{
	void boss_init();
}