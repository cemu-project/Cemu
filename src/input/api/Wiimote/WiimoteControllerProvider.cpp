#include "input/api/Wiimote/WiimoteControllerProvider.h"
#include "input/api/Wiimote/NativeWiimoteController.h"
#include "input/api/Wiimote/WiimoteMessages.h"

#ifdef HAS_HIDAPI
#include "input/api/Wiimote/hidapi/HidapiWiimote.h"
#elif BOOST_OS_WINDOWS
#include "input/api/Wiimote/windows/WinWiimoteDevice.h"
#endif

#include <numbers>

WiimoteControllerProvider::WiimoteControllerProvider()
	: m_running(true)
{
	m_reader_thread = std::thread(&WiimoteControllerProvider::reader_thread, this);
	m_writer_thread = std::thread(&WiimoteControllerProvider::writer_thread, this);
}

WiimoteControllerProvider::~WiimoteControllerProvider()
{
	if (m_running)
	{
		m_running = false;
		m_writer_thread.join();
		m_reader_thread.join();
	}
}

std::vector<std::shared_ptr<ControllerBase>> WiimoteControllerProvider::get_controllers()
{
	std::scoped_lock lock(m_device_mutex);
	for (const auto& device : WiimoteDevice_t::get_devices())
	{
		// test connection of all devices as they might have been changed
		const bool is_connected = device->write_data({kStatusRequest, 0x00});
		if (is_connected)
		{
			// only add unknown, connected devices to our list
			const bool is_new_device = std::none_of(m_wiimotes.cbegin(), m_wiimotes.cend(),
			                                        [device](const auto& it) { return *it.device == *device; });
			if (is_new_device)
			{
				m_wiimotes.push_back(std::make_unique<Wiimote>(device));
			}
		}
	}

	std::vector<std::shared_ptr<ControllerBase>> result;
	for (size_t i = 0; i < m_wiimotes.size(); ++i)
	{
		result.emplace_back(std::make_shared<NativeWiimoteController>(i));
	}

	return result;
}

bool WiimoteControllerProvider::is_connected(size_t index)
{
	std::shared_lock lock(m_device_mutex);
	return index < m_wiimotes.size() && m_wiimotes[index].connected;
}

bool WiimoteControllerProvider::is_registered_device(size_t index)
{
	std::shared_lock lock(m_device_mutex);
	return index < m_wiimotes.size();
}

void WiimoteControllerProvider::set_rumble(size_t index, bool state)
{
	std::shared_lock lock(m_device_mutex);
	if (index >= m_wiimotes.size())
		return;
	
	m_wiimotes[index].rumble = state;
	lock.unlock();

	send_packet(index, { kStatusRequest, 0x00 });
}

void WiimoteControllerProvider::request_status(size_t index)
{
	send_packet(index, {kStatusRequest, 0x00});
}

void WiimoteControllerProvider::set_led(size_t index, size_t player_index)
{
	uint8 mask = 0;
	mask |= 1 << (4 + (player_index % 4));
	if (player_index >= 4)
		mask |= 1 << (4 + ((player_index - 3) % 4));
	send_packet(index, {kLED, mask});
}

uint32 WiimoteControllerProvider::get_packet_delay(size_t index)
{
	std::shared_lock lock(m_device_mutex);
	if (index < m_wiimotes.size())
	{
		return m_wiimotes[index].data_delay;
	}

	return kDefaultPacketDelay;
}

void WiimoteControllerProvider::set_packet_delay(size_t index, uint32 delay)
{
	std::shared_lock lock(m_device_mutex);
	if (index < m_wiimotes.size())
	{
		m_wiimotes[index].data_delay = delay;
	}
}

WiimoteControllerProvider::WiimoteState WiimoteControllerProvider::get_state(size_t index)
{
	std::shared_lock lock(m_device_mutex);
	if (index < m_wiimotes.size())
	{
		std::shared_lock data_lock(m_wiimotes[index].mutex);
		return m_wiimotes[index].state;
	}

	return {};
}

void WiimoteControllerProvider::reader_thread()
{
	SetThreadName("WiimoteControllerProvider::reader_thread");
	std::chrono::steady_clock::time_point lastCheck = {};
	while (m_running.load(std::memory_order_relaxed))
	{
		const auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck) > std::chrono::seconds(2))
		{
			// check for new connected wiimotes
			get_controllers();
			lastCheck = std::chrono::steady_clock::now();
		}

		bool receivedAnyPacket = false;
		std::shared_lock lock(m_device_mutex);
		for (size_t index = 0; index < m_wiimotes.size(); ++index)
		{
			auto& wiimote = m_wiimotes[index];
			if (!wiimote.connected)
				continue;

			const auto read_data = wiimote.device->read_data();
			if (!read_data || read_data->empty())
				continue;
			receivedAnyPacket = true;

			std::shared_lock read_lock(wiimote.mutex);
			WiimoteState new_state = wiimote.state;
			read_lock.unlock();

			bool update_report = false;

			const uint8* data = read_data->data();
			const auto id = (InputReportId)*data;
			++data;
			switch (id)
			{
			case kStatus:
				{
                    cemuLog_logDebug(LogType::Force,"WiimoteControllerProvider::read_thread: kStatus");
					new_state.buttons = (*(uint16*)data) & (~0x60E0);
					data += 2;
					new_state.flags = *data;
					++data;
					data += 2; // skip zeroes
					new_state.battery_level = *data;
					++data;

					new_state.ir_camera.mode = set_ir_camera(index, true);
					if(!new_state.m_calibrated)
						calibrate(index);

					if(!new_state.m_motion_plus)
						detect_motion_plus(index);

					if (HAS_FLAG(new_state.flags, kExtensionConnected))
					{
                        cemuLog_logDebug(LogType::Force,"Extension flag is set");
						if(new_state.m_extension.index() == 0)
							request_extension(index);
					}
					else
					{
						new_state.m_extension = {};
					}

					update_report = true;
				}
				break;
			case kRead:
				{
                    cemuLog_logDebug(LogType::Force,"WiimoteControllerProvider::read_thread: kRead");
					new_state.buttons = (*(uint16*)data) & (~0x60E0);
					data += 2;
					const uint8 error_flag = *data & 0xF, size = (*data >> 4) + 1;
					++data;

					if (error_flag)
					{

						// 7 means that wiimote is already enabled or not available
                        cemuLog_logDebug(LogType::Force,"Received error on data read {:#x}", error_flag);
						continue;
					}

					auto address = *(betype<uint16>*)data;
					data += 2;
					if (address == (kRegisterCalibration & 0xFFFF))
					{
                        cemuLog_logDebug(LogType::Force,"Calibration received");

						cemu_assert(size == 8);

						new_state.m_calib_acceleration.zero.x = (uint16)*data << 2;
						++data;
						new_state.m_calib_acceleration.zero.y = (uint16)*data << 2;
						++data;
						new_state.m_calib_acceleration.zero.z = (uint16)*data << 2;
						++data;
						// --XXYYZZ
						new_state.m_calib_acceleration.zero.x |= (*data >> 4) & 0x3; // 5|4 -> 1|0
						new_state.m_calib_acceleration.zero.y |= (*data >> 2) & 0x3; // 3|4 -> 1|0
						new_state.m_calib_acceleration.zero.z |= *data & 0x3;
						++data;

						new_state.m_calib_acceleration.gravity.x = (uint16)*data << 2;
						++data;
						new_state.m_calib_acceleration.gravity.y = (uint16)*data << 2;
						++data;
						new_state.m_calib_acceleration.gravity.z = (uint16)*data << 2;
						++data;
						new_state.m_calib_acceleration.gravity.x |= (*data >> 4) & 0x3; // 5|4 -> 1|0
						new_state.m_calib_acceleration.gravity.y |= (*data >> 2) & 0x3; // 3|4 -> 1|0
						new_state.m_calib_acceleration.gravity.z |= *data & 0x3;
						++data;

						new_state.m_calibrated = true;
					}
					else if (address == (kRegisterExtensionType & 0xFFFF))
					{
						if (size == 0xf)
						{
							cemuLog_logDebug(LogType::Force,"Extension type received but no extension connected");
							continue;
						}

						cemu_assert(size == 6);
						auto be_type = *(betype<uint64>*)data;
						data += 6; // 48
						be_type >>= 16;
						be_type &= 0xFFFFFFFFFFFF;
						switch (be_type.value())
						{
						case kExtensionNunchuck:
                            cemuLog_logDebug(LogType::Force,"Extension Type Received: Nunchuck");
							new_state.m_extension = NunchuckData{};
							break;
						case kExtensionClassic:
                            cemuLog_logDebug(LogType::Force,"Extension Type Received: Classic");
							new_state.m_extension = ClassicData{};
							break;
						case kExtensionClassicPro:
                            cemuLog_logDebug(LogType::Force,"Extension Type Received: Classic Pro");
                            break;
						case kExtensionGuitar:
                            cemuLog_logDebug(LogType::Force,"Extension Type Received: Guitar");
                            break;
						case kExtensionDrums:
                            cemuLog_logDebug(LogType::Force,"Extension Type Received: Drums");
                            break;
						case kExtensionBalanceBoard:
                            cemuLog_logDebug(LogType::Force,"Extension Type Received: Balance Board");
                            break;
						case kExtensionMotionPlus:
                            cemuLog_logDebug(LogType::Force,"Extension Type Received: MotionPlus");
							set_motion_plus(index, true);
							new_state.m_motion_plus = MotionPlusData{};
							break;
						case kExtensionPartialyInserted:
                            cemuLog_logDebug(LogType::Force,"Extension only partially inserted");
							new_state.m_extension = {};
							request_status(index);
							break;
						default:
                            cemuLog_logDebug(LogType::Force,"Unknown extension: {:#x}", be_type.value());
                            new_state.m_extension = {};
							break;
						}

						if (new_state.m_extension.index() != 0)
							send_read_packet(index, kRegisterMemory, kRegisterExtensionCalibration, 0x10);
					}
					else if (address == (kRegisterExtensionCalibration & 0xFFFF))
					{
						cemu_assert(size == 0x10);
                        cemuLog_logDebug(LogType::Force,"Extension calibration received");
						std::visit(
							overloaded
							{
								[](auto)
								{
								},
								[data](MotionPlusData& mp)
								{
									// TODO fix
								},
								[data](NunchuckData& nunchuck)
								{
									std::array<uint8, 14> zero{};
									if (memcmp(zero.data(), data, zero.size()) == 0)
									{
                                        cemuLog_logDebug(LogType::Force,"Extension calibration data is zero");
										return;
									}

									nunchuck.calibration.zero.x = (uint16)data[0] << 2;
									nunchuck.calibration.zero.y = (uint16)data[1] << 2;
									nunchuck.calibration.zero.z = (uint16)data[2] << 2;
									// --XXYYZZ
									nunchuck.calibration.zero.x |= (data[3] >> 4) & 0x3; // 5|4 -> 1|0
									nunchuck.calibration.zero.y |= (data[3] >> 2) & 0x3; // 3|4 -> 1|0
									nunchuck.calibration.zero.z |= data[3] & 0x3;

									nunchuck.calibration.gravity.x = (uint16)data[4] << 2;;
									nunchuck.calibration.gravity.y = (uint16)data[5] << 2;;
									nunchuck.calibration.gravity.z = (uint16)data[6] << 2;;
									// --XXYYZZ
									nunchuck.calibration.gravity.x |= (data[7] >> 4) & 0x3; // 5|4 -> 1|0
									nunchuck.calibration.gravity.y |= (data[7] >> 2) & 0x3; // 3|4 -> 1|0
									nunchuck.calibration.gravity.z |= data[7] & 0x3;

									nunchuck.calibration.max.x = data[8];
									nunchuck.calibration.max.y = data[11];

									nunchuck.calibration.min.x = data[9];
									nunchuck.calibration.min.y = data[12];

									nunchuck.calibration.center.x = data[10];
									nunchuck.calibration.center.y = data[13];
								}
							}, new_state.m_extension);
					}
					else
					{
                        cemuLog_logDebug(LogType::Force,"Unhandled read data received");
                        continue;
					}

					update_report = true;
				}
				break;
            case kAcknowledge:
                {
                    new_state.buttons = *(uint16*)data & (~0x60E0);
                    data += 2;
                    const auto report_id = *data++;
                    const auto error = *data++;
                    if (error)
                        cemuLog_logDebug(LogType::Force, "Error {:#x} from output report {:#x}", error, report_id);
                    break;
                }
			case kDataCore:
				{
					// 30 BB BB
					new_state.buttons = *(uint16*)data & (~0x60E0);
					data += 2;
					break;
				}
			case kDataCoreAcc:
				{
					// 31 BB BB AA AA AA
					new_state.buttons = *(uint16*)data & (~0x60E0);
					parse_acceleration(new_state, data);
					break;
				}
			case kDataCoreExt8:
				{
					// 32 BB BB EE EE EE EE EE EE EE EE
					new_state.buttons = *(uint16*)data & (~0x60E0);
					data += 2;
					break;
				}
			case kDataCoreAccIR:
				{
					// 33 BB BB AA AA AA II II II II II II II II II II II II 
					new_state.buttons = *(uint16*)data & (~0x60E0);
					parse_acceleration(new_state, data);
					data += parse_ir(new_state, data);
					break;
				}
			case kDataCoreExt19:
				{
					// 34 BB BB EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE 
					new_state.buttons = *(uint16*)data & (~0x60E0);
					data += 2;
					break;
				}
			case kDataCoreAccExt:
				{
					// 35 BB BB AA AA AA EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE
					new_state.buttons = *(uint16*)data & (~0x60E0);
					parse_acceleration(new_state, data);
					break;
				}
			case kDataCoreIRExt:
				{
					// 36 BB BB II II II II II II II II II II EE EE EE EE EE EE EE EE EE
					new_state.buttons = *(uint16*)data & (~0x60E0);
					data += 2;
					break;
				}
			case kDataCoreAccIRExt:
				{
					// 37 BB BB AA AA AA II II II II II II II II II II EE EE EE EE EE EE
					new_state.buttons = *(uint16*)data & (~0x60E0);
					parse_acceleration(new_state, data);
					data += parse_ir(new_state, data); // 10

					std::visit(
						overloaded
						{
							[](auto)
							{
							},
							[data](MotionPlusData& mp) mutable
							{
								glm::vec<3, uint16> raw;
								raw.x = *data;
								++data;
								raw.y = *data;
								++data;
								raw.z = *data;
								++data;

								raw.x |= (uint16)*data << 6; // 7|2 -> 13|8
								mp.slow_yaw = *data & 2;
								mp.slow_pitch = *data & 1;
								++data;

								raw.y |= (uint16)*data << 6; // 7|2 -> 13|8
								mp.slow_roll = *data & 2;
								mp.extension_connected = *data & 1;
								++data;

								raw.z |= (uint16)*data << 6; // 7|2 -> 13|8

								auto& calib = mp.calibration;

								glm::vec3 orientation = raw;
								/*orientation -= calib.zero;
	
								Vector3<float> tmp = calib.gravity;
								tmp -= calib.zero;
								orientation /= tmp;*/

								mp.orientation = orientation;
                                cemuLog_logDebug(LogType::Force,"MotionPlus: {:.2f}, {:.2f} {:.2f}", mp.orientation.x, mp.orientation.y, mp.orientation.z);
							},
							[data](NunchuckData& nunchuck) mutable
							{
								nunchuck.raw_axis.x = *data;
								++data;
								nunchuck.raw_axis.y = *data;
								++data;

								glm::vec<3, uint16> raw_acc;
								raw_acc.x = (uint16)*data << 2;
								++data;
								raw_acc.y = (uint16)*data << 2;
								++data;
								raw_acc.z = (uint16)*data << 2;
								++data;
								nunchuck.z = (*data & 1) == 0;
								nunchuck.c = (*data & 2) == 0;

								raw_acc.x |= (*data >> 2) & 0x3; // 3|2 -> 1|0
								raw_acc.y |= (*data >> 4) & 0x3; // 5|4 -> 1|0
								raw_acc.z |= (*data >> 6) & 0x3; // 7|6 -> 1|0

								auto& calib = nunchuck.calibration;

								if (nunchuck.raw_axis.x < nunchuck.calibration.center.x) // [-1, 0]
									nunchuck.axis.x = ((float)nunchuck.raw_axis.x - calib.min.x) / ((float)nunchuck.
										calibration.center.x - calib.min.x + 0.012f) - 1.0f;
								else // [0, 1]
									nunchuck.axis.x = (float)(nunchuck.raw_axis.x - nunchuck.calibration.center.x) / (
										nunchuck.calibration.max.x - nunchuck.calibration.center.x + 0.012f);

								if (nunchuck.raw_axis.y <= nunchuck.calibration.center.y) // [-1, 0]
									nunchuck.axis.y = ((float)nunchuck.raw_axis.y - calib.min.y) / ((float)nunchuck.
										calibration.center.y - calib.min.y + 0.012f) - 1.0f;
								else // [0, 1]
									nunchuck.axis.y = (float)(nunchuck.raw_axis.y - nunchuck.calibration.center.y) / (
										nunchuck.calibration.max.y - nunchuck.calibration.center.y);

								glm::vec3 acceleration = raw_acc;
								nunchuck.prev_acceleration = nunchuck.acceleration;
								nunchuck.acceleration = acceleration - glm::vec3(calib.zero);

								float acc[3]{ -nunchuck.acceleration.x, -nunchuck.acceleration.z, nunchuck.acceleration.y };
								const auto grav = nunchuck.calibration.gravity - nunchuck.calibration.zero;

								auto tacc = nunchuck.acceleration;
								auto pacc = nunchuck.prev_acceleration;
								if (grav != glm::vec<3, uint16>{})
								{
									acc[0] /= (float)grav.x;
									acc[1] /= (float)grav.y;
									acc[2] /= (float)grav.z;

									tacc.x /= (float)grav.x;
									pacc.x /= (float)grav.x;

									tacc.y /= (float)grav.y;
									pacc.y /= (float)grav.y;

									tacc.z /= (float)grav.z;
									pacc.z /= (float)grav.z;
								}
								float zero3[3]{};
								float zero4[4]{};


								nunchuck.motion_sample = MotionSample(
									acc,
									glm::length(tacc - pacc),
									zero3,
									zero3,
									zero4
								);
                                cemuLog_logDebug(LogType::Force,"Nunchuck: Z={}, C={} | {}, {} | {:.2f}, {:.2f}, {:.2f}",
                                                 nunchuck.z, nunchuck.c,
                                                 nunchuck.axis.x, nunchuck.axis.y,
                                                 RadToDeg(nunchuck.acceleration.x), RadToDeg(nunchuck.acceleration.y),
                                                 RadToDeg(nunchuck.acceleration.z));
							},
							[data](ClassicData& classic) mutable
							{
								classic.left_raw_axis.x = *data & 0x3F;
								classic.right_raw_axis.x = (*data & 0xC0) >> 3; // 7|6 -> 4|3
								++data;

								classic.left_raw_axis.y = *data & 0x3F;
								classic.right_raw_axis.x |= (*data & 0xC0) >> 5; // 7|6 -> 2|1
								++data;

								classic.right_raw_axis.y = *data & 0x1F;
								classic.raw_trigger.x = (*data & 0x60) >> 2; // 6|5 -> 4|3
								classic.right_raw_axis.x |= (*data & 0x80) >> 7; // 7 -> 0
								++data;

								classic.raw_trigger.x |= (*data & 0xE0) >> 5; // 7|5 -> 2|0
								classic.raw_trigger.y = (*data & 0x1F);
								++data;

								classic.buttons = ~(*(uint16*)data);
								data += 2;

								classic.left_axis = classic.left_raw_axis;
								classic.left_axis /= 63.0f;
								classic.left_axis = classic.left_axis * 2.0f - 1.0f;

								classic.right_axis = classic.right_raw_axis;
								classic.right_axis /= 31.0f;
								classic.right_axis = classic.right_axis * 2.0f - 1.0f;

								classic.trigger = classic.raw_trigger;
								classic.trigger /= 31.0f;
                                cemuLog_logDebug(LogType::Force,"Classic Controller: Buttons={:b} | {}, {} | {}, {} | {}, {}",
                                                 classic.buttons, classic.left_axis.x, classic.left_axis.y,
                                                 classic.right_axis.x, classic.right_axis.y, classic.trigger.x,
                                                 classic.trigger.y);

							}
						}, new_state.m_extension);


					break;
				}
			case kDataExt:
				{
					// 3d EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE
					break;
				}
			default:
                cemuLog_logDebug(LogType::Force,"unhandled input packet id {} for wiimote {}", id, index);
			}

			// update motion data
			//const auto motionnow = std::chrono::high_resolution_clock::now();
			//const auto delta_time = (float)std::chrono::duration_cast<std::chrono::milliseconds>(motionnow - new_state.m_last_motion_timestamp).count() / 1000.0f;
			//new_state.m_last_motion_timestamp = motionnow;

			float acc[3]{-new_state.m_acceleration.x, -new_state.m_acceleration.z, new_state.m_acceleration.y};
			const auto grav = new_state.m_calib_acceleration.gravity - new_state.m_calib_acceleration.zero;

			auto tacc = new_state.m_acceleration;
			auto pacc = new_state.m_prev_acceleration;
			if (grav != glm::vec<3, uint16>{})
			{
				acc[0] /= (float)grav.x;
				acc[1] /= (float)grav.y;
				acc[2] /= (float)grav.z;

				tacc.x /= (float)grav.x;
				pacc.x /= (float)grav.x;

				tacc.y /= (float)grav.y;
				pacc.y /= (float)grav.y;

				tacc.z /= (float)grav.z;
				pacc.z /= (float)grav.z;
			}
			float zero3[3]{};
			float zero4[4]{};


			new_state.motion_sample = MotionSample(
				acc,
				glm::length(tacc - pacc),
				zero3,
				zero3,
				zero4
			);

			std::unique_lock data_lock(wiimote.mutex);
			wiimote.state = new_state;
			data_lock.unlock();

			if (update_report)
				update_report_type(index);
		}

		lock.unlock();
		if (!receivedAnyPacket)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

void WiimoteControllerProvider::parse_acceleration(WiimoteState& wiimote_state, const uint8*& data)
{
	glm::vec<3, uint16> raw_acc;
	// acc encoded in BB BB
	raw_acc.x = (*data >> 5) & 3; // 6|5 -> 1|0
	++data;
	raw_acc.y = (*data >> 4) & 2; // 5 -> 1
	raw_acc.z = (*data >> 5) & 2; // 6 -> 1
	++data;

	raw_acc.x |= (uint16)*data << 2;
	++data;
	raw_acc.y |= (uint16)*data << 2;
	++data;
	raw_acc.z |= (uint16)*data << 2;
	++data;

	wiimote_state.m_prev_acceleration = wiimote_state.m_acceleration;
	glm::vec3 acceleration = raw_acc;

	const auto& calib = wiimote_state.m_calib_acceleration;
	wiimote_state.m_acceleration = acceleration - glm::vec3(calib.zero);

	glm::vec3 tmp = calib.gravity;
	tmp -= calib.zero;
	acceleration = (wiimote_state.m_acceleration / tmp);

	const float pi_2 = (float)std::numbers::pi / 2.0f;
	wiimote_state.m_roll = std::atan2(acceleration.z, acceleration.x) - pi_2;
}

void WiimoteControllerProvider::rotate_ir(WiimoteState& wiimote_state)
{
	const float rot = wiimote_state.m_roll;
	if (rot == 0.0f)
		return;

	const float sin = std::sin(rot);
	const float cos = std::cos(rot);
	int i = -1;
	for (auto& dot : wiimote_state.ir_camera.dots)
	{
		i++;
		if (!dot.visible)
			continue;
		// move to center, rotate and move back
		dot.pos -= 0.5f;
		dot.pos.x = (dot.pos.x * cos) + (dot.pos.y * (-sin));
		dot.pos.y = (dot.pos.x * sin) + (dot.pos.y * cos);
		dot.pos += 0.5f;
	}
}

void WiimoteControllerProvider::calculate_ir_position(WiimoteState& wiimote_state)
{
	auto& ir = wiimote_state.ir_camera;
	ir.m_prev_position = ir.position;

	std::pair indices = ir.indices;
	if (ir.middle.x != 0)
	{
		const float last_angle = std::atan(ir.middle.y / ir.middle.x);
		float best_distance = std::numeric_limits<float>::max();
		for (size_t i = 0; i < ir.dots.size(); ++i)
		{
			if (!ir.dots[i].visible)
				continue;

			for (size_t j = i + 1; j < ir.dots.size(); ++j)
			{
				if (!ir.dots[j].visible)
					continue;

				const auto mid = (ir.dots[i].pos + ir.dots[j].pos) / 2.0f;
				if (mid.x == 0)
					continue;

				// check if angle is close enough to the last known one
				float angle = std::atan(mid.y / mid.x);
				if (std::abs(last_angle - angle) > DegToRad(10.0f))
					continue;

				// check if distance between points is similar to last known distance
				const float distance = std::abs(ir.distance - glm::length(ir.dots[i].pos - ir.dots[j].pos));
				if (distance > 0.1f && distance > best_distance)
					continue;

				// found a new pair
				best_distance = distance;
				indices = {(sint32)i, (sint32)j};
			}
		}
	}

	if (ir.dots[indices.first].visible && ir.dots[indices.second].visible)
	{
		ir.prev_dots[indices.first] = ir.dots[indices.first];
		ir.prev_dots[indices.second] = ir.dots[indices.second];
		ir.position = (ir.dots[indices.first].pos + ir.dots[indices.second].pos) / 2.0f;

		ir.middle = ir.position;
		ir.distance = glm::length(ir.dots[indices.first].pos - ir.dots[indices.second].pos);
		ir.indices = indices;
	}
	else if (ir.dots[indices.first].visible)
	{
		ir.position = ir.middle + (ir.dots[indices.first].pos - ir.prev_dots[indices.first].pos);
	}
	else if (ir.dots[indices.second].visible)
	{
		ir.position = ir.middle + (ir.dots[indices.second].pos - ir.prev_dots[indices.second].pos);
	}
}


sint32 WiimoteControllerProvider::parse_ir(WiimoteState& wiimote_state, const uint8* data)
{
	switch (wiimote_state.ir_camera.mode)
	{
	case kIRDisabled:
		wiimote_state.ir_camera.dots = {};
		return 0;
	case kBasicIR:
		{
			const auto ir = (BasicIR*)data;
			for (int i = 0; i < 2; ++i)
			{
				auto& dot1 = wiimote_state.ir_camera.dots[i * 2];
				auto& dot2 = wiimote_state.ir_camera.dots[i * 2 + 1];

				dot1.raw.x = ir[i].x1 | (ir[i].bits.x1 << 8); // 9|8
				dot1.raw.y = ir[i].y1 | (ir[i].bits.y1 << 8);
				dot1.size = 0;

				dot2.raw.x = ir[i].x2 | (ir[i].bits.x2 << 8);
				dot2.raw.y = ir[i].y2 | (ir[i].bits.y2 << 8);
				dot2.size = 0;

				dot1.visible = dot1.raw != glm::vec<2, uint16>(0x3ff, 0x3ff);
				if (dot1.visible)
					dot1.pos = glm::vec2(1.0f - dot1.raw.x / 1023.0f, (float)dot1.raw.y / 768.0f);
				else
					dot1.pos = {};

				dot2.visible = dot2.raw != glm::vec<2, uint16>(0x3ff, 0x3ff);
				if (dot2.visible)
					dot2.pos = glm::vec2(1.0f - dot2.raw.x / 1023.0f, (float)dot2.raw.y / 768.0f);
				else
					dot2.pos = {};
			}

			rotate_ir(wiimote_state);
			calculate_ir_position(wiimote_state);
			return sizeof(BasicIR) * 2;
		}
	case kExtendedIR:
		{
			const auto ir = (ExtendedIR*)data;
			for (int i = 0; i < 4; ++i)
			{
				auto& dot = wiimote_state.ir_camera.dots[i];
				dot.raw.x = ir[i].x;
				dot.raw.y = ir[i].y;

				dot.raw.x |= (uint16)ir[i].bits.x << 8; // 9|8
				dot.raw.y |= (uint16)ir[i].bits.y << 8; // 9|8

				dot.size = ir[i].bits.size;

				dot.visible = dot.raw != glm::vec<2, uint16>(0x3ff, 0x3ff);
				if (dot.visible)
					dot.pos = glm::vec2(1.0f - dot.raw.x / 1023.0f, (float)dot.raw.y / 768.0f);
				else
					dot.pos = {};
			}

			rotate_ir(wiimote_state);
			calculate_ir_position(wiimote_state);
			return sizeof(ExtendedIR) * 4;
		}
	default:
		cemu_assert(false);
		break;
	}
	return 0;
}

void WiimoteControllerProvider::request_extension(size_t index)
{
	// send_write_packet(index, kRegisterMemory, kRegisterExtensionEncrypted, { 0x00 });
	send_write_packet(index, kRegisterMemory, kRegisterExtension1, {0x55});
	send_write_packet(index, kRegisterMemory, kRegisterExtension2, {0x00});
	send_read_packet(index, kRegisterMemory, kRegisterExtensionType, 6);
}

void WiimoteControllerProvider::detect_motion_plus(size_t index)
{
	send_read_packet(index, kRegisterMemory, kRegisterMotionPlusDetect, 6);
}

void WiimoteControllerProvider::set_motion_plus(size_t index, bool state)
{
	if (state) {
		send_write_packet(index, kRegisterMemory, kRegisterMotionPlusInit, { 0x55 });
		send_write_packet(index, kRegisterMemory, kRegisterMotionPlusEnable, { 0x04 });
	}
	else
	{
		send_write_packet(index, kRegisterMemory, kRegisterExtension1, { 0x55 });
	}
}


void WiimoteControllerProvider::writer_thread()
{
	SetThreadName("WiimoteControllerProvider::writer_thread");
	while (m_running.load(std::memory_order_relaxed))
	{
		std::unique_lock writer_lock(m_writer_mutex);
		while (m_write_queue.empty())
		{
			if (m_writer_cond.wait_for(writer_lock, std::chrono::milliseconds(250)) == std::cv_status::timeout)
			{
				if (!m_running.load(std::memory_order_relaxed))
					return;
			}
		}

		auto index = (size_t)-1;
		std::vector<uint8> data;
		std::shared_lock device_lock(m_device_mutex);

		// get first packet of device which is ready to be sent
		const auto now = std::chrono::high_resolution_clock::now();
		for (auto it = m_write_queue.begin(); it != m_write_queue.end(); ++it)
		{
			if (it->first >= m_wiimotes.size())
				continue;

			const auto delay = m_wiimotes[it->first].data_delay.load(std::memory_order_relaxed);
			if (now >= m_wiimotes[it->first].data_ts + std::chrono::milliseconds(delay))
			{
				index = it->first;
				data = it->second;
				m_write_queue.erase(it);
				break;
			}
		}
		writer_lock.unlock();

		if (index != (size_t)-1 && !data.empty())
		{
			if (m_wiimotes[index].rumble)
				data[1] |= 1;

			m_wiimotes[index].connected = m_wiimotes[index].device->write_data(data);
			if (m_wiimotes[index].connected)
			{
				m_wiimotes[index].data_ts = std::chrono::high_resolution_clock::now();
			}
			else
			{
				m_wiimotes[index].rumble = false;
			}
		}
		device_lock.unlock();

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::this_thread::yield();
	}
}

void WiimoteControllerProvider::calibrate(size_t index)
{
	send_read_packet(index, kEEPROMMemory, kRegisterCalibration, 8);
}

void WiimoteControllerProvider::update_report_type(size_t index)
{
	std::shared_lock read_lock(m_wiimotes[index].mutex);
	auto& state = m_wiimotes[index].state;

	const bool extension = state.m_extension.index() != 0; // TODO || HasMotionPlus();
	const bool ir = state.ir_camera.mode != kIRDisabled;
	const bool motion = true; // UseMotion();

	InputReportId report_type;
	if (extension && ir && motion)
		report_type = kDataCoreAccIRExt;
	else if (extension && ir)
		report_type = kDataCoreIRExt;
	else if (extension && motion)
		report_type = kDataCoreAccExt;
	else if (ir && motion)
		report_type = kDataCoreAccIR;
	else if (extension)
		report_type = kDataCoreExt19;
	else if (ir)
		report_type = kDataCoreAccIR;
	else if (motion)
		report_type = kDataCoreAcc;
	else
		report_type = kDataCore;

    cemuLog_logDebug(LogType::Force,"Setting report type to {}", report_type);
	send_packet(index, {kType, 0x04, report_type});

	state.ir_camera.mode = set_ir_camera(index, true);
}

IRMode WiimoteControllerProvider::set_ir_camera(size_t index, bool state)
{
	std::shared_lock read_lock(m_wiimotes[index].mutex);
	auto& wiimote_state = m_wiimotes[index].state;

	IRMode mode;
	if (!state)
		mode = kIRDisabled;
	else
	{
		mode = wiimote_state.m_extension.index() == 0 ? kExtendedIR : kBasicIR;
	}

	if (wiimote_state.ir_camera.mode == mode)
		return mode;

	wiimote_state.ir_camera.mode = mode;

	const uint8_t data = state ? 0x04 : 0x00;
	send_packet(index, {kIR, data});
	send_packet(index, {kIR2, data});
	if (state)
	{
		send_write_packet(index, kRegisterMemory, kRegisterIR, {0x08});
		send_write_packet(index, kRegisterMemory, kRegisterIRSensitivity1,
		                  {0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xaa, 0x00, 0x64});
		send_write_packet(index, kRegisterMemory, kRegisterIRSensitivity2, {0x63, 0x03});
		send_write_packet(index, kRegisterMemory, kRegisterIRMode, {(uint8)mode});
		send_write_packet(index, kRegisterMemory, kRegisterIR, {0x08});
	}

	update_report_type(index);
	return mode;
}

void WiimoteControllerProvider::send_packet(size_t index, std::vector<uint8> data)
{
	cemu_assert(data.size() > 1);

	std::shared_lock device_lock(m_device_mutex);
	if (index >= m_wiimotes.size())
		return;

	device_lock.unlock();

	std::unique_lock lock(m_writer_mutex);
	m_write_queue.emplace_back(index, data);
	m_writer_cond.notify_one();
}

void WiimoteControllerProvider::send_read_packet(size_t index, MemoryType type, RegisterAddress address, uint16 size)
{
	std::vector<uint8> data(7);
	data[0] = kReadMemory;
	data[1] = type;
	*(betype<uint32>*)(data.data() + 2) = (address & 0xFFFFFF) << 8; // only uint24
	*(betype<uint16>*)(data.data() + 2 + 3) = size;

	send_packet(index, std::move(data));
}

void WiimoteControllerProvider::send_write_packet(size_t index, MemoryType type, RegisterAddress address,
                                                  const std::vector<uint8>& data)
{
	cemu_assert(data.size() <= 16);
	std::vector<uint8> packet(6 + 16);
	packet[0] = kWriteMemory;
	packet[1] = type;
	*(betype<uint32>*)(packet.data() + 2) = (address & 0xFFFFFF) << 8; // only uint24
	*(packet.data() + 2 + 3) = (uint8)data.size();
	std::copy(data.begin(), data.end(), packet.data() + 2 + 3 + 1);
	send_packet(index, std::move(packet));
}
