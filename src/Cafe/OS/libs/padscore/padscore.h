#pragma once

#include "util/math/vector3.h"

namespace padscore
{
	void start();
	void load();
}

constexpr int kWPADMaxControllers = 4;
constexpr int kKPADMaxControllers = 7;

#define WPAD_ERR_NONE				0
#define WPAD_ERR_NO_CONTROLLER		-1
#define WPAD_ERR_BUSY				-2
#define WPAD_ERR_INVALID			-4

#define WPAD_PRESS_UNITS			4

#define WPAD_FMT_CORE				0
#define WPAD_FMT_CORE_ACC			1
#define WPAD_FMT_CORE_ACC_DPD		2

typedef struct
{
	float x;
	float y;
	float z;
}padVec3D_t;

static_assert(sizeof(padVec3D_t) == 0xC);

typedef struct beVec3D
{
	float32be x;
	float32be y;
	float32be z;

	beVec3D() = default;

	beVec3D(const Vector3<float>& v)
		: x(v.x), y(v.y), z(v.z) {}

	beVec3D(const float32be& x, const float32be& y, const float32be& z)
		: x(x), y(y), z(z) {}

}beVec3D_t;

static_assert(sizeof(beVec3D_t) == 0xC);

typedef struct
{
	float x;
	float y;
}padVec2D_t;

static_assert(sizeof(padVec2D_t) == 0x8);

typedef struct
{
	float32be x;
	float32be y;
}beVec2D_t;

static_assert(sizeof(beVec2D_t) == 0x8);

union KPADEXStatus_t
{
	struct
	{
		beVec2D_t stick;
		beVec3D_t acc;
		float32be accValue;
		float32be accSpeed;
	}fs;
	struct
	{
		uint32be hold;
		uint32be trig;
		uint32be release;
		beVec2D_t lstick;
		beVec2D_t rstick;
		float32be ltrigger;
		float32be rtrigger;
	}cl;
	struct
	{
		uint32be hold;
		uint32be trig;
		uint32be release;
		beVec2D_t lstick;
		beVec2D_t rstick;
		uint32be charge;
		uint32be cable;
	}uc;
	struct
	{
		uint32be hold;
		uint32be trig;
		uint32be release;
	}cm;
	uint8 _padding[0x50];
};

static_assert(sizeof(KPADEXStatus_t) == 0x50);

struct KPADMPDir_t
{
	padVec3D_t X;
	padVec3D_t Y;
	padVec3D_t Z;
};

static_assert(sizeof(KPADMPDir_t) == 0x24);

struct KPADMPStatus_t
{
	padVec3D_t mpls;
	padVec3D_t angle;
	KPADMPDir_t dir;
};

static_assert(sizeof(KPADMPStatus_t) == 0x3C);

struct KPADStatus_t
{
	uint32be hold;
	uint32be trig;
	uint32be release;
	beVec3D_t acc;
	float32be acc_value;
	float32be acc_speed;
	beVec2D_t pos;
	beVec2D_t vec;
	float32be speed;
	beVec2D_t horizon;
	beVec2D_t horiVec;
	float32be horiSpeed;
	float32be dist;
	float32be distVec;
	float32be distSpeed;
	beVec2D_t accVertical;
	uint8 devType;
	sint8 wpadErr;
	sint8 dpd_valid_fg;
	uint8 data_format;
	KPADEXStatus_t ex_status;
	KPADMPStatus_t mpls;
	uint8 _unused[4];
};

static_assert(sizeof(KPADStatus_t) == 0xF0);

#define WPAD_DPD_MAX_OBJECTS (4)

struct DPDObject_t
{ 
	uint16be x;
	uint16be y;
	uint16be size;
	uint8 traceId;
	uint8 padding;
};

static_assert(sizeof(DPDObject_t) == 0x8);

struct WPADStatus_t
{
	uint16be button;
	uint16be accX;
	uint16be accY;
	uint16be accZ;
	DPDObject_t obj[WPAD_DPD_MAX_OBJECTS];
	uint8 dev;
	sint8 err;
};

static_assert(sizeof(WPADStatus_t) == 0x2A);

struct WPADFSStatus_t : WPADStatus_t
{
	uint16be fsAccX;
	uint16be fsAccY;
	uint16be fsAccZ;
	sint8 fsStickX;
	sint8 fsStickY;
};

struct WPADCLStatus_t : WPADStatus_t
{
	uint16be clButton;
	uint16be clLStickX;
	uint16be clLStickY;
	uint16be clRStickX;
	uint16be clRStickY;
	uint8 clTriggerL;
	uint8 clTriggerR;
};

struct WPADUCStatus_t : WPADStatus_t
{
	uint8 padding1[2]; // unsure
	uint32be ucButton;
	uint16be ucLStickX;
	uint16be ucLStickY;
	uint16be ucRStickX;
	uint16be ucRStickY;
	uint32be charge;
	uint32be cable;
};

static_assert(sizeof(WPADUCStatus_t) == 0x40);

struct WPADTRStatus_t : WPADStatus_t
{
	uint16 trButton;
	uint8 brake;
	uint8 mascon;
};

struct WPADBLStatus_t : WPADStatus_t
{
	uint16be press[WPAD_PRESS_UNITS];
	sint8 temp;
	uint8 battery;
} ;

static_assert(sizeof(WPADBLStatus_t) == 0x34);

struct WPADMPStatus_t : WPADStatus_t
{
	union
	{
		// for Nunchuk
		struct
		{
			uint16be fsAccX;
			uint16be fsAccY;
			uint16be fsAccZ;
			sint8 fsStickX;
			sint8 fsStickY;
		}fs;

		// for Classic
		struct
		{
			uint16be clButton;
			uint16be clLStickX;
			uint16be clLStickY;
			uint16be clRStickX;
			uint16be clRStickY;
			uint8 clTriggerL;
			uint8 clTriggerR;
		}cl;
	}status;
	uint8 stat;
	uint8 _ukn;
	uint16be pitch;
	uint16be yaw;
	uint16be roll;
};

static_assert(sizeof(WPADMPStatus_t) == 0x3E);

typedef struct KPADUnifiedWpadStatus
{
	union
	{
		WPADStatus_t core;
		WPADFSStatus_t fs;
		WPADCLStatus_t cl;
		WPADTRStatus_t tr;
		WPADBLStatus_t bl;
		WPADMPStatus_t mp;
	}u;
	uint8 fmt;
	uint8 padding;
	uint32 _40; // padding?
}KPADUnifiedWpadStatus_t;

static_assert(sizeof(KPADUnifiedWpadStatus_t) == 0x44);

