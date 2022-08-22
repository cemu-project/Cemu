#pragma once

#include "Cafe/OS/libs/padscore/padscore.h"

namespace vpad
{
	void load();
	void start();
}

#define VPAD_MAX_CONTROLLERS (2)

struct BtnRepeat
{
	sint32 delay, pulse;
};

enum VPADTouchValidity
{
	kTpValid = 0,
	kTpInvalidX = 1, // only x invalid
	kTpInvalidY = 2, // only y invalid
	kTpInvalid = kTpInvalidX | kTpInvalidY,
};

enum VPADTouchState
{
	kTpTouchOff = 0,
	kTpTouchOn = 1,
};

struct VPADDir
{
	beVec3D_t x;
	beVec3D_t y;
	beVec3D_t z;

	VPADDir() = default;
	VPADDir(const beVec3D_t& x, const beVec3D_t& y, const beVec3D_t& z)
		: x(x), y(y), z(z) {}

};

static_assert(sizeof(VPADDir) == 0x24);

struct VPADTPData_t
{
	uint16be x;
	uint16be y;
	uint16be touch;
	uint16be validity;
};

static_assert(sizeof(VPADTPData_t) == 8);

typedef struct VPADStatus
{
	/* +0x00 */ uint32be hold;
	/* +0x04 */ uint32be trig;
	/* +0x08 */ uint32be release;
	/* +0x0C */ beVec2D_t leftStick;
	/* +0x14 */ beVec2D_t rightStick;
	/* +0x1C */ beVec3D_t acc;
	/* +0x28 */ float32be accMagnitude;
	/* +0x2C */ float32be accAcceleration;
	/* +0x30 */ beVec2D_t accXY;
	/* +0x38 */ beVec3D_t gyroChange;
	/* +0x44 */ beVec3D_t gyroOrientation;
	/* +0x50 */ sint8 vpadErr;
	/* +0x51 */ uint8 padding1[1];
	/* +0x52 */ VPADTPData_t tpData;
	/* +0x5A */ VPADTPData_t tpProcessed1;
	/* +0x62 */ VPADTPData_t tpProcessed2;
	/* +0x6A */	uint8 padding2[2];
	/* +0x6C */ VPADDir dir;
	/* +0x90 */ uint8 headphoneStatus;
	/* +0x91 */ uint8 padding3[3];
	/* +0x94 */ beVec3D_t magnet;
	/* +0xA0 */ uint8 slideVolume;
	/* +0xA1 */ uint8 batteryLevel;
	/* +0xA2 */ uint8 micStatus;
	/* +0xA3 */ uint8 slideVolume2;
	/* +0xA4 */ uint8 padding4[8];
}VPADStatus_t;

static_assert(sizeof(VPADStatus) == 0xAC);
