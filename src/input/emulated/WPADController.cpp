#include <api/Controller.h>
#include "input/emulated/WPADController.h"

#include "input/emulated/ClassicController.h"
#include "input/emulated/ProController.h"
#include "input/emulated/WiimoteController.h"

WPADController::WPADController(size_t player_index, WPADDataFormat data_format)
	: EmulatedController(player_index), m_data_format(data_format)
{
}

WPADDataFormat WPADController::get_default_data_format() const
{
	switch (get_device_type())
	{
	case kWAPDevCore:
		return kDataFormat_CORE_ACC_DPD;
	case kWAPDevFreestyle:
		return kDataFormat_FREESTYLE_ACC;
	case kWAPDevClassic:
		return kDataFormat_CLASSIC;
	case kWAPDevMPLS:
		return kDataFormat_MPLS;
	case kWAPDevMPLSFreeStyle:
		return kDataFormat_FREESTYLE_ACC_DPD;
	case kWAPDevMPLSClassic:
		return kDataFormat_CLASSIC_ACC_DPD;
	case kWAPDevURCC:
		return kDataFormat_URCC;
	default:
		return kDataFormat_CORE;
	}
}

uint32 WPADController::get_emulated_button_flag(WPADDataFormat format, uint32 id) const
{
	switch(format)
	{
	case kDataFormat_CORE:
	case kDataFormat_CORE_ACC:
	case kDataFormat_CORE_ACC_DPD:
	case kDataFormat_CORE_ACC_DPD_FULL:
	case kDataFormat_FREESTYLE:
	case kDataFormat_FREESTYLE_ACC:
	case kDataFormat_FREESTYLE_ACC_DPD:
	case kDataFormat_MPLS:
		return WiimoteController::s_get_emulated_button_flag(id);
	case kDataFormat_CLASSIC:
	case kDataFormat_CLASSIC_ACC:
	case kDataFormat_CLASSIC_ACC_DPD:
		return ClassicController::s_get_emulated_button_flag(id);
	
	case kDataFormat_TRAIN: break;
	case kDataFormat_GUITAR: break;
	case kDataFormat_BALANCE_CHECKER: break;
	case kDataFormat_DRUM: break;
	
	case kDataFormat_TAIKO: break;
	case kDataFormat_URCC:
		return ProController::s_get_emulated_button_flag(id);

	}

	return 0;
}

void WPADController::WPADRead(WPADStatus_t* status)
{
	controllers_update_states();
	uint32 button = 0;
	for (uint32 i = 1; i < get_highest_mapping_id(); ++i)
	{
		if (is_mapping_down(i))
		{
			const uint32 value = get_emulated_button_flag(m_data_format, i);
			button |= value;
		}
	}

	m_homebutton_down |= is_home_down();

	// todo fill position api from wiimote

	switch (m_data_format)
	{
	case kDataFormat_CORE:
	case kDataFormat_CORE_ACC:
	case kDataFormat_CORE_ACC_DPD:
	case kDataFormat_CORE_ACC_DPD_FULL:
	{
		memset(status, 0x00, sizeof(*status));
		status->button = button;
		break;
	}

	case kDataFormat_FREESTYLE:
	case kDataFormat_FREESTYLE_ACC:
	case kDataFormat_FREESTYLE_ACC_DPD:
	{
		WPADFSStatus_t* ex_status = (WPADFSStatus_t*)status;
		memset(ex_status, 0x00, sizeof(*ex_status));
		ex_status->button = button;

		auto axis = get_axis();
		axis *= 127.0f;
		ex_status->fsStickX = (sint8)axis.x;
		ex_status->fsStickY = (sint8)axis.y;
		break;
	}

	case kDataFormat_CLASSIC:
	case kDataFormat_CLASSIC_ACC:
	case kDataFormat_CLASSIC_ACC_DPD:
	case kDataFormat_GUITAR:
	case kDataFormat_DRUM:
	case kDataFormat_TAIKO:
	{
		WPADCLStatus_t* ex_status = (WPADCLStatus_t*)status;
		memset(ex_status, 0x00, sizeof(*ex_status));
		ex_status->clButton = button;
		
		auto axis = get_axis();
		axis *= 2048.0f;
		ex_status->clLStickX = (uint16)axis.x;
		ex_status->clLStickY = (uint16)axis.y;

		auto rotation = get_rotation();
		rotation *= 2048.0f;
		ex_status->clRStickX = (uint16)rotation.x;
		ex_status->clRStickY = (uint16)rotation.y;
		break;
	}
	case kDataFormat_TRAIN:
	{
		WPADTRStatus_t* ex_status = (WPADTRStatus_t*)status;
		// TODO
		break;
	}
	case kDataFormat_BALANCE_CHECKER:
	{
		WPADBLStatus_t* ex_status = (WPADBLStatus_t*)status;
		// TODO
		break;
	}
	case kDataFormat_MPLS:
	{
		WPADMPStatus_t* ex_status = (WPADMPStatus_t*)status;
		ex_status->stat = 1; // attached
		// TODO
		break;
	}
	case kDataFormat_URCC:
	{
		WPADUCStatus_t* ex_status = (WPADUCStatus_t*)status;
		memset(ex_status, 0x00, sizeof(*ex_status));
		ex_status->ucButton = button;

		ex_status->cable = TRUE;
		ex_status->charge = TRUE;

		auto axis = get_axis();
		axis *= 2048.0f;
		ex_status->ucLStickX = (uint16)axis.x;
		ex_status->ucLStickY = (uint16)axis.y;

		auto rotation = get_rotation();
		rotation *= 2048.0f;
		ex_status->ucRStickX = (uint16)rotation.x;
		ex_status->ucRStickY = (uint16)rotation.y;

		break;
	}
	default:
		cemu_assert(false);
	}

	status->dev = get_device_type();
	status->err = WPAD_ERR_NONE;
}

void WPADController::KPADRead(KPADStatus_t& status, const BtnRepeat& repeat)
{
	uint32be* hold, *release, *trigger;
	switch (type())
	{
	case Pro:
		hold = &status.ex_status.uc.hold;
		release = &status.ex_status.uc.release;
		trigger = &status.ex_status.uc.trig;
		break;
	case Classic:
		hold = &status.ex_status.cl.hold;
		release = &status.ex_status.cl.release;
		trigger = &status.ex_status.cl.trig;
		break;
	default:
		hold = &status.hold;
		release = &status.release;
		trigger = &status.trig;
	}

	controllers_update_states();
	for (uint32 i = 1; i < get_highest_mapping_id(); ++i)
	{
		if (is_mapping_down(i))
		{
			const uint32 value = get_emulated_button_flag(m_data_format, i);
			*hold |= value;
		}
	}

	m_homebutton_down |= is_home_down();
	
	// button repeat
	const auto now = std::chrono::steady_clock::now();
	if (*hold != m_last_holdvalue)
	{
		m_last_hold_change = m_last_pulse = now;
	}

	if (repeat.pulse > 0)
	{
		if (m_last_hold_change + std::chrono::milliseconds(repeat.delay) >= now)
		{
			if ((m_last_pulse + std::chrono::milliseconds(repeat.pulse)) < now)
			{
				m_last_pulse = now;
				*hold |= kWPADButtonRepeat;
			}
		}
	}

	// axis
	const auto axis = get_axis();
	const auto rotation = get_rotation();

	*release = m_last_holdvalue & ~*hold;
	//status.release = m_last_holdvalue & ~*hold;
	*trigger = ~m_last_holdvalue & *hold;
	//status.trig = ~m_last_holdvalue & *hold;
	m_last_holdvalue = *hold;

	if (is_mpls_attached())
	{
		status.mpls.dir.X.x = 1;
		status.mpls.dir.X.y = 0;
		status.mpls.dir.X.z = 0;

		status.mpls.dir.Y.x = 0;
		status.mpls.dir.Y.y = 1;
		status.mpls.dir.Y.z = 0;

		status.mpls.dir.Z.x = 0;
		status.mpls.dir.Z.y = 0;
		status.mpls.dir.Z.z = 1;
	}

	if (has_motion())
	{
		auto motion_sample = get_motion_data();

		glm::vec3 acc;
		motion_sample.getAccelerometer(&acc[0]);
		status.acc.x = acc.x;
		status.acc.y = acc.y;
		status.acc.z = acc.z;

		status.acc_value = motion_sample.getVPADAccMagnitude();
		status.acc_speed = motion_sample.getVPADAccAcceleration();

		//glm::vec2 acc_vert;
		//motion_sample.getVPADAccXY(&acc_vert[0]);
		//status.acc_vertical.x = acc_vert.x;
		//status.acc_vertical.y = acc_vert.y;

		status.accVertical.x = std::min(1.0f, std::abs(acc.x + acc.y));
		status.accVertical.y = std::min(std::max(-1.0f, -acc.z), 1.0f);

		if (is_mpls_attached())
		{
			// todo
			glm::vec3 gyroChange;
			motion_sample.getVPADGyroChange(&gyroChange[0]);
			//const auto& gyroChange = motionSample.getVPADGyroChange();
			status.mpls.mpls.x = gyroChange.x;
			status.mpls.mpls.y = gyroChange.y;
			status.mpls.mpls.z = gyroChange.z;

			//debug_printf("GyroChange %7.2lf %7.2lf %7.2lf\n", (float)status.gyroChange.x, (float)status.gyroChange.y, (float)status.gyroChange.z);

			glm::vec3 gyroOrientation;
			motion_sample.getVPADOrientation(&gyroOrientation[0]);
			//const auto& gyroOrientation = motionSample.getVPADOrientation();
			status.mpls.angle.x = gyroOrientation.x;
			status.mpls.angle.y = gyroOrientation.y;
			status.mpls.angle.z = gyroOrientation.z;

			float attitude[9];
			motion_sample.getVPADAttitudeMatrix(attitude);
			status.mpls.dir.X.x = attitude[0];
			status.mpls.dir.X.y = attitude[1];
			status.mpls.dir.X.z = attitude[2];
			status.mpls.dir.Y.x = attitude[3];
			status.mpls.dir.Y.y = attitude[4];
			status.mpls.dir.Y.z = attitude[5];
			status.mpls.dir.Z.x = attitude[6];
			status.mpls.dir.Z.y = attitude[7];
			status.mpls.dir.Z.z = attitude[8];
		}
	}
	auto visibility = GetPositionVisibility();
	if (has_position() && visibility != PositionVisibility::NONE)
	{
		if (visibility == PositionVisibility::FULL)
			status.dpd_valid_fg = 2;
		else
			status.dpd_valid_fg = -1;

		const auto position = get_position();

		const auto pos = (position * 2.0f) - 1.0f;
		status.pos.x = pos.x;
		status.pos.y = pos.y;

		const auto delta = position - get_prev_position();
		status.vec.x = delta.x;
		status.vec.y = delta.y;
		status.speed = glm::length(delta);
	}
	else
		status.dpd_valid_fg = 0;

	switch (type())
	{
	case Wiimote:
		status.ex_status.fs.stick.x = axis.x;
		status.ex_status.fs.stick.y = axis.y;

		if(has_second_motion())
		{
			auto motion_sample = get_second_motion_data();

			glm::vec3 acc;
			motion_sample.getAccelerometer(&acc[0]);
			status.ex_status.fs.acc.x = acc.x;
			status.ex_status.fs.acc.y = acc.y;
			status.ex_status.fs.acc.z = acc.z;

			status.ex_status.fs.accValue = motion_sample.getVPADAccMagnitude();
			status.ex_status.fs.accSpeed = motion_sample.getVPADAccAcceleration();
		}

		break;
	case Pro:
		status.ex_status.uc.lstick.x = axis.x;
		status.ex_status.uc.lstick.y = axis.y;

		status.ex_status.uc.rstick.x = rotation.x;
		status.ex_status.uc.rstick.y = rotation.y;

		status.ex_status.uc.charge = FALSE;
		status.ex_status.uc.cable = TRUE;

		break;
	case Classic:
		status.ex_status.cl.lstick.x = axis.x;
		status.ex_status.cl.lstick.y = axis.y;

		status.ex_status.cl.rstick.x = rotation.x;
		status.ex_status.cl.rstick.y = rotation.y;

		if (HAS_FLAG((uint32)status.ex_status.cl.hold, kCLButton_ZL))
			status.ex_status.cl.ltrigger = 1.0f;

		if (HAS_FLAG((uint32)status.ex_status.cl.hold, kCLButton_ZR))
			status.ex_status.cl.rtrigger = 1.0f;
		break;
	default:
		cemu_assert(false);
	}

	
}
