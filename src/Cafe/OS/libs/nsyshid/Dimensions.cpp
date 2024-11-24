#include "Dimensions.h"

#include "nsyshid.h"
#include "Backend.h"

#include "Common/FileStream.h"

#include <array>
#include <random>

namespace nsyshid
{
	static constexpr std::array<uint8, 16> COMMAND_KEY = {0x55, 0xFE, 0xF6, 0xB0, 0x62, 0xBF, 0x0B, 0x41,
														  0xC9, 0xB3, 0x7C, 0xB4, 0x97, 0x3E, 0x29, 0x7B};

	static constexpr std::array<uint8, 17> CHAR_CONSTANT = {0xB7, 0xD5, 0xD7, 0xE6, 0xE7, 0xBA, 0x3C, 0xA8,
															0xD8, 0x75, 0x47, 0x68, 0xCF, 0x23, 0xE9, 0xFE, 0xAA};

	static constexpr std::array<uint8, 25> PWD_CONSTANT = {0x28, 0x63, 0x29, 0x20, 0x43, 0x6F, 0x70, 0x79,
														   0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0x4C, 0x45,
														   0x47, 0x4F, 0x20, 0x32, 0x30, 0x31, 0x34, 0xAA, 0xAA};

	DimensionsUSB g_dimensionstoypad;

	const std::map<const uint32, const char*> s_listMinis = {
		{0, "Blank Tag"},
		{1, "Batman"},
		{2, "Gandalf"},
		{3, "Wyldstyle"},
		{4, "Aquaman"},
		{5, "Bad Cop"},
		{6, "Bane"},
		{7, "Bart Simpson"},
		{8, "Benny"},
		{9, "Chell"},
		{10, "Cole"},
		{11, "Cragger"},
		{12, "Cyborg"},
		{13, "Cyberman"},
		{14, "Doc Brown"},
		{15, "The Doctor"},
		{16, "Emmet"},
		{17, "Eris"},
		{18, "Gimli"},
		{19, "Gollum"},
		{20, "Harley Quinn"},
		{21, "Homer Simpson"},
		{22, "Jay"},
		{23, "Joker"},
		{24, "Kai"},
		{25, "ACU Trooper"},
		{26, "Gamer Kid"},
		{27, "Krusty the Clown"},
		{28, "Laval"},
		{29, "Legolas"},
		{30, "Lloyd"},
		{31, "Marty McFly"},
		{32, "Nya"},
		{33, "Owen Grady"},
		{34, "Peter Venkman"},
		{35, "Slimer"},
		{36, "Scooby-Doo"},
		{37, "Sensei Wu"},
		{38, "Shaggy"},
		{39, "Stay Puft"},
		{40, "Superman"},
		{41, "Unikitty"},
		{42, "Wicked Witch of the West"},
		{43, "Wonder Woman"},
		{44, "Zane"},
		{45, "Green Arrow"},
		{46, "Supergirl"},
		{47, "Abby Yates"},
		{48, "Finn the Human"},
		{49, "Ethan Hunt"},
		{50, "Lumpy Space Princess"},
		{51, "Jake the Dog"},
		{52, "Harry Potter"},
		{53, "Lord Voldemort"},
		{54, "Michael Knight"},
		{55, "B.A. Baracus"},
		{56, "Newt Scamander"},
		{57, "Sonic the Hedgehog"},
		{58, "Future Update (unreleased)"},
		{59, "Gizmo"},
		{60, "Stripe"},
		{61, "E.T."},
		{62, "Tina Goldstein"},
		{63, "Marceline the Vampire Queen"},
		{64, "Batgirl"},
		{65, "Robin"},
		{66, "Sloth"},
		{67, "Hermione Granger"},
		{68, "Chase McCain"},
		{69, "Excalibur Batman"},
		{70, "Raven"},
		{71, "Beast Boy"},
		{72, "Betelgeuse"},
		{73, "Lord Vortech (unreleased)"},
		{74, "Blossom"},
		{75, "Bubbles"},
		{76, "Buttercup"},
		{77, "Starfire"},
		{78, "World 15 (unreleased)"},
		{79, "World 16 (unreleased)"},
		{80, "World 17 (unreleased)"},
		{81, "World 18 (unreleased)"},
		{82, "World 19 (unreleased)"},
		{83, "World 20 (unreleased)"},
		{768, "Unknown 768"},
		{769, "Supergirl Red Lantern"},
		{770, "Unknown 770"}};

	const std::map<const uint32, const char*> s_listTokens = {
		{1000, "Police Car"},
		{1001, "Aerial Squad Car"},
		{1002, "Missile Striker"},
		{1003, "Gravity Sprinter"},
		{1004, "Street Shredder"},
		{1005, "Sky Clobberer"},
		{1006, "Batmobile"},
		{1007, "Batblaster"},
		{1008, "Sonic Batray"},
		{1009, "Benny's Spaceship"},
		{1010, "Lasercraft"},
		{1011, "The Annihilator"},
		{1012, "DeLorean Time Machine"},
		{1013, "Electric Time Machine"},
		{1014, "Ultra Time Machine"},
		{1015, "Hoverboard"},
		{1016, "Cyclone Board"},
		{1017, "Ultimate Hoverjet"},
		{1018, "Eagle Interceptor"},
		{1019, "Eagle Sky Blazer"},
		{1020, "Eagle Swoop Diver"},
		{1021, "Swamp Skimmer"},
		{1022, "Cragger's Fireship"},
		{1023, "Croc Command Sub"},
		{1024, "Cyber-Guard"},
		{1025, "Cyber-Wrecker"},
		{1026, "Laser Robot Walker"},
		{1027, "K-9"},
		{1028, "K-9 Ruff Rover"},
		{1029, "K-9 Laser Cutter"},
		{1030, "TARDIS"},
		{1031, "Laser-Pulse TARDIS"},
		{1032, "Energy-Burst TARDIS"},
		{1033, "Emmet's Excavator"},
		{1034, "Destroy Dozer"},
		{1035, "Destruct-o-Mech"},
		{1036, "Winged Monkey"},
		{1037, "Battle Monkey"},
		{1038, "Commander Monkey"},
		{1039, "Axe Chariot"},
		{1040, "Axe Hurler"},
		{1041, "Soaring Chariot"},
		{1042, "Shelob the Great"},
		{1043, "8-Legged Stalker"},
		{1044, "Poison Slinger"},
		{1045, "Homer's Car"},
		{1047, "The SubmaHomer"},
		{1046, "The Homercraft"},
		{1048, "Taunt-o-Vision"},
		{1050, "The MechaHomer"},
		{1049, "Blast Cam"},
		{1051, "Velociraptor"},
		{1053, "Venom Raptor"},
		{1052, "Spike Attack Raptor"},
		{1054, "Gyrosphere"},
		{1055, "Sonic Beam Gyrosphere"},
		{1056, " Gyrosphere"},
		{1057, "Clown Bike"},
		{1058, "Cannon Bike"},
		{1059, "Anti-Gravity Rocket Bike"},
		{1060, "Mighty Lion Rider"},
		{1061, "Lion Blazer"},
		{1062, "Fire Lion"},
		{1063, "Arrow Launcher"},
		{1064, "Seeking Shooter"},
		{1065, "Triple Ballista"},
		{1066, "Mystery Machine"},
		{1067, "Mystery Tow & Go"},
		{1068, "Mystery Monster"},
		{1069, "Boulder Bomber"},
		{1070, "Boulder Blaster"},
		{1071, "Cyclone Jet"},
		{1072, "Storm Fighter"},
		{1073, "Lightning Jet"},
		{1074, "Electro-Shooter"},
		{1075, "Blade Bike"},
		{1076, "Flight Fire Bike"},
		{1077, "Blades of Fire"},
		{1078, "Samurai Mech"},
		{1079, "Samurai Shooter"},
		{1080, "Soaring Samurai Mech"},
		{1081, "Companion Cube"},
		{1082, "Laser Deflector"},
		{1083, "Gold Heart Emitter"},
		{1084, "Sentry Turret"},
		{1085, "Turret Striker"},
		{1086, "Flight Turret Carrier"},
		{1087, "Scooby Snack"},
		{1088, "Scooby Fire Snack"},
		{1089, "Scooby Ghost Snack"},
		{1090, "Cloud Cuckoo Car"},
		{1091, "X-Stream Soaker"},
		{1092, "Rainbow Cannon"},
		{1093, "Invisible Jet"},
		{1094, "Laser Shooter"},
		{1095, "Torpedo Bomber"},
		{1096, "NinjaCopter"},
		{1097, "Glaciator"},
		{1098, "Freeze Fighter"},
		{1099, "Travelling Time Train"},
		{1100, "Flight Time Train"},
		{1101, "Missile Blast Time Train"},
		{1102, "Aqua Watercraft"},
		{1103, "Seven Seas Speeder"},
		{1104, "Trident of Fire"},
		{1105, "Drill Driver"},
		{1106, "Bane Dig 'n' Drill"},
		{1107, "Bane Drill 'n' Blast"},
		{1108, "Quinn Mobile"},
		{1109, "Quinn Ultra Racer"},
		{1110, "Missile Launcher"},
		{1111, "The Joker's Chopper"},
		{1112, "Mischievous Missile Blaster"},
		{1113, "Lock 'n' Laser Jet"},
		{1114, "Hover Pod"},
		{1115, "Krypton Striker"},
		{1116, "Super Stealth Pod"},
		{1117, "Dalek"},
		{1118, "Fire 'n' Ride Dalek"},
		{1119, "Silver Shooter Dalek"},
		{1120, "Ecto-1"},
		{1121, "Ecto-1 Blaster"},
		{1122, "Ecto-1 Water Diver"},
		{1123, "Ghost Trap"},
		{1124, "Ghost Stun 'n' Trap"},
		{1125, "Proton Zapper"},
		{1126, "Unknown"},
		{1127, "Unknown"},
		{1128, "Unknown"},
		{1129, "Unknown"},
		{1130, "Unknown"},
		{1131, "Unknown"},
		{1132, "Lloyd's Golden Dragon"},
		{1133, "Sword Projector Dragon"},
		{1134, "Unknown"},
		{1135, "Unknown"},
		{1136, "Unknown"},
		{1137, "Unknown"},
		{1138, "Unknown"},
		{1139, "Unknown"},
		{1140, "Unknown"},
		{1141, "Unknown"},
		{1142, "Unknown"},
		{1143, "Unknown"},
		{1144, "Mega Flight Dragon"},
		{1145, "Unknown"},
		{1146, "Unknown"},
		{1147, "Unknown"},
		{1148, "Unknown"},
		{1149, "Unknown"},
		{1150, "Unknown"},
		{1151, "Unknown"},
		{1152, "Unknown"},
		{1153, "Unknown"},
		{1154, "Unknown"},
		{1155, "Flying White Dragon"},
		{1156, "Golden Fire Dragon"},
		{1157, "Ultra Destruction Dragon"},
		{1158, "Arcade Machine"},
		{1159, "8-Bit Shooter"},
		{1160, "The Pixelator"},
		{1161, "G-6155 Spy Hunter"},
		{1162, "Interdiver"},
		{1163, "Aerial Spyhunter"},
		{1164, "Slime Shooter"},
		{1165, "Slime Exploder"},
		{1166, "Slime Streamer"},
		{1167, "Terror Dog"},
		{1168, "Terror Dog Destroyer"},
		{1169, "Soaring Terror Dog"},
		{1170, "Ancient Psychic Tandem War Elephant"},
		{1171, "Cosmic Squid"},
		{1172, "Psychic Submarine"},
		{1173, "BMO"},
		{1174, "DOGMO"},
		{1175, "SNAKEMO"},
		{1176, "Jakemobile"},
		{1177, "Snail Dude Jake"},
		{1178, "Hover Jake"},
		{1179, "Lumpy Car"},
		{1181, "Lumpy Land Whale"},
		{1180, "Lumpy Truck"},
		{1182, "Lunatic Amp"},
		{1183, "Shadow Scorpion"},
		{1184, "Heavy Metal Monster"},
		{1185, "B.A.'s Van"},
		{1186, "Fool Smasher"},
		{1187, "Pain Plane"},
		{1188, "Phone Home"},
		{1189, "Mobile Uplink"},
		{1190, "Super-Charged Satellite"},
		{1191, "Niffler"},
		{1192, "Sinister Scorpion"},
		{1193, "Vicious Vulture"},
		{1194, "Swooping Evil"},
		{1195, "Brutal Bloom"},
		{1196, "Crawling Creeper"},
		{1197, "Ecto-1 (2016)"},
		{1198, "Ectozer"},
		{1199, "PerfEcto"},
		{1200, "Flash 'n' Finish"},
		{1201, "Rampage Record Player"},
		{1202, "Stripe's Throne"},
		{1203, "R.C. Racer"},
		{1204, "Gadget-O-Matic"},
		{1205, "Scarlet Scorpion"},
		{1206, "Hogwarts Express"},
		{1208, "Steam Warrior"},
		{1207, "Soaring Steam Plane"},
		{1209, "Enchanted Car"},
		{1210, "Shark Sub"},
		{1211, "Monstrous Mouth"},
		{1212, "IMF Scrambler"},
		{1213, "Shock Cycle"},
		{1214, "IMF Covert Jet"},
		{1215, "IMF Sports Car"},
		{1216, "IMF Tank"},
		{1217, "IMF Splorer"},
		{1218, "Sonic Speedster"},
		{1219, "Blue Typhoon"},
		{1220, "Moto Bug"},
		{1221, "The Tornado"},
		{1222, "Crabmeat"},
		{1223, "Eggcatcher"},
		{1224, "K.I.T.T."},
		{1225, "Goliath Armored Semi"},
		{1226, "K.I.T.T. Jet"},
		{1227, "Police Helicopter"},
		{1228, "Police Hovercraft"},
		{1229, "Police Plane"},
		{1230, "Bionic Steed"},
		{1231, "Bat-Raptor"},
		{1232, "Ultrabat"},
		{1233, "Batwing"},
		{1234, "The Black Thunder"},
		{1235, "Bat-Tank"},
		{1236, "Skeleton Organ"},
		{1237, "Skeleton Jukebox"},
		{1238, "Skele-Turkey"},
		{1239, "One-Eyed Willy's Pirate Ship"},
		{1240, "Fanged Fortune"},
		{1241, "Inferno Cannon"},
		{1242, "Buckbeak"},
		{1243, "Giant Owl"},
		{1244, "Fierce Falcon"},
		{1245, "Saturn's Sandworm"},
		{1247, "Haunted Vacuum"},
		{1246, "Spooky Spider"},
		{1248, "PPG Smartphone"},
		{1249, "PPG Hotline"},
		{1250, "Powerpuff Mag-Net"},
		{1253, "Mega Blast Bot"},
		{1251, "Ka-Pow Cannon"},
		{1252, "Slammin' Guitar"},
		{1254, "Octi"},
		{1255, "Super Skunk"},
		{1256, "Sonic Squid"},
		{1257, "T-Car"},
		{1258, "T-Forklift"},
		{1259, "T-Plane"},
		{1260, "Spellbook of Azarath"},
		{1261, "Raven Wings"},
		{1262, "Giant Hand"},
		{1263, "Titan Robot"},
		{1264, "T-Rocket"},
		{1265, "Robot Retriever"}};

	DimensionsToypadDevice::DimensionsToypadDevice()
		: Device(0x0E6F, 0x0241, 1, 2, 0)
	{
		m_IsOpened = false;
	}

	bool DimensionsToypadDevice::Open()
	{
		if (!IsOpened())
		{
			m_IsOpened = true;
		}
		return true;
	}

	void DimensionsToypadDevice::Close()
	{
		if (IsOpened())
		{
			m_IsOpened = false;
		}
	}

	bool DimensionsToypadDevice::IsOpened()
	{
		return m_IsOpened;
	}

	Device::ReadResult DimensionsToypadDevice::Read(ReadMessage* message)
	{
		memcpy(message->data, g_dimensionstoypad.GetStatus().data(), message->length);
		message->bytesRead = message->length;
		return Device::ReadResult::Success;
	}

	Device::WriteResult DimensionsToypadDevice::Write(WriteMessage* message)
	{
		if (message->length != 32)
			return Device::WriteResult::Error;

		g_dimensionstoypad.SendCommand(std::span<const uint8, 32>{message->data, 32});
		message->bytesWritten = message->length;
		return Device::WriteResult::Success;
	}

	bool DimensionsToypadDevice::GetDescriptor(uint8 descType,
											   uint8 descIndex,
											   uint8 lang,
											   uint8* output,
											   uint32 outputMaxLength)
	{
		uint8 configurationDescriptor[0x29];

		uint8* currentWritePtr;

		// configuration descriptor
		currentWritePtr = configurationDescriptor + 0;
		*(uint8*)(currentWritePtr + 0) = 9;			// bLength
		*(uint8*)(currentWritePtr + 1) = 2;			// bDescriptorType
		*(uint16be*)(currentWritePtr + 2) = 0x0029; // wTotalLength
		*(uint8*)(currentWritePtr + 4) = 1;			// bNumInterfaces
		*(uint8*)(currentWritePtr + 5) = 1;			// bConfigurationValue
		*(uint8*)(currentWritePtr + 6) = 0;			// iConfiguration
		*(uint8*)(currentWritePtr + 7) = 0x80;		// bmAttributes
		*(uint8*)(currentWritePtr + 8) = 0xFA;		// MaxPower
		currentWritePtr = currentWritePtr + 9;
		// configuration descriptor
		*(uint8*)(currentWritePtr + 0) = 9;	   // bLength
		*(uint8*)(currentWritePtr + 1) = 0x04; // bDescriptorType
		*(uint8*)(currentWritePtr + 2) = 0;	   // bInterfaceNumber
		*(uint8*)(currentWritePtr + 3) = 0;	   // bAlternateSetting
		*(uint8*)(currentWritePtr + 4) = 2;	   // bNumEndpoints
		*(uint8*)(currentWritePtr + 5) = 3;	   // bInterfaceClass
		*(uint8*)(currentWritePtr + 6) = 0;	   // bInterfaceSubClass
		*(uint8*)(currentWritePtr + 7) = 0;	   // bInterfaceProtocol
		*(uint8*)(currentWritePtr + 8) = 0;	   // iInterface
		currentWritePtr = currentWritePtr + 9;
		// configuration descriptor
		*(uint8*)(currentWritePtr + 0) = 9;			// bLength
		*(uint8*)(currentWritePtr + 1) = 0x21;		// bDescriptorType
		*(uint16be*)(currentWritePtr + 2) = 0x0111; // bcdHID
		*(uint8*)(currentWritePtr + 4) = 0x00;		// bCountryCode
		*(uint8*)(currentWritePtr + 5) = 0x01;		// bNumDescriptors
		*(uint8*)(currentWritePtr + 6) = 0x22;		// bDescriptorType
		*(uint16be*)(currentWritePtr + 7) = 0x001D; // wDescriptorLength
		currentWritePtr = currentWritePtr + 9;
		// endpoint descriptor 1
		*(uint8*)(currentWritePtr + 0) = 7;		  // bLength
		*(uint8*)(currentWritePtr + 1) = 0x05;	  // bDescriptorType
		*(uint8*)(currentWritePtr + 2) = 0x81;	  // bEndpointAddress
		*(uint8*)(currentWritePtr + 3) = 0x03;	  // bmAttributes
		*(uint16be*)(currentWritePtr + 4) = 0x40; // wMaxPacketSize
		*(uint8*)(currentWritePtr + 6) = 0x01;	  // bInterval
		currentWritePtr = currentWritePtr + 7;
		// endpoint descriptor 2
		*(uint8*)(currentWritePtr + 0) = 7;		  // bLength
		*(uint8*)(currentWritePtr + 1) = 0x05;	  // bDescriptorType
		*(uint8*)(currentWritePtr + 1) = 0x02;	  // bEndpointAddress
		*(uint8*)(currentWritePtr + 2) = 0x03;	  // bmAttributes
		*(uint16be*)(currentWritePtr + 3) = 0x40; // wMaxPacketSize
		*(uint8*)(currentWritePtr + 5) = 0x01;	  // bInterval
		currentWritePtr = currentWritePtr + 7;

		cemu_assert_debug((currentWritePtr - configurationDescriptor) == 0x29);

		memcpy(output, configurationDescriptor,
			   std::min<uint32>(outputMaxLength, sizeof(configurationDescriptor)));
		return true;
	}

	bool DimensionsToypadDevice::SetProtocol(uint8 ifIndex, uint8 protocol)
	{
		cemuLog_log(LogType::Force, "Toypad Protocol");
		return true;
	}

	bool DimensionsToypadDevice::SetReport(ReportMessage* message)
	{
		cemuLog_log(LogType::Force, "Toypad Report");
		return true;
	}

	std::array<uint8, 32> DimensionsUSB::GetStatus()
	{
		std::array<uint8, 32> response = {};

		bool responded = false;
		do
		{
			if (!m_queries.empty())
			{
				response = m_queries.front();
				m_queries.pop();
				responded = true;
			}
			else if (!m_figureAddedRemovedResponses.empty() && m_isAwake)
			{
				std::lock_guard lock(m_dimensionsMutex);
				response = m_figureAddedRemovedResponses.front();
				m_figureAddedRemovedResponses.pop();
				responded = true;
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
		while (responded == false);
		return response;
	}

	void DimensionsUSB::SendCommand(std::span<const uint8, 32> buf)
	{
		const uint8 command = buf[2];
		const uint8 sequence = buf[3];

		std::array<uint8, 32> q_result{};

		switch (command)
		{
		case 0xB0: // Wake
		{
			// Consistent device response to the wake command
			q_result = {0x55, 0x0e, 0x01, 0x28, 0x63, 0x29,
						0x20, 0x4c, 0x45, 0x47, 0x4f, 0x20,
						0x32, 0x30, 0x31, 0x34, 0x46};
			break;
		}
		case 0xB1: // Seed
		{
			// Initialise a random number generator using the seed provided
			g_dimensionstoypad.GenerateRandomNumber(std::span<const uint8, 8>{buf.begin() + 4, 8}, sequence, q_result);
			break;
		}
		case 0xB3: // Challenge
		{
			// Get the next number in the sequence based on the RNG from 0xB1 command
			g_dimensionstoypad.GetChallengeResponse(std::span<const uint8, 8>{buf.begin() + 4, 8}, sequence, q_result);
			break;
		}
		case 0xC0: // Color
		case 0xC1: // Get Pad Color
		case 0xC2: // Fade
		case 0xC3: // Flash
		case 0xC4: // Fade Random
		case 0xC6: // Fade All
		case 0xC7: // Flash All
		case 0xC8: // Color All
		{
			// Send a blank response to acknowledge color has been sent to toypad
			q_result = {0x55, 0x01, sequence};
			q_result[3] = GenerateChecksum(q_result, 3);
			break;
		}
		case 0xD2: // Read
		{
			// Read 4 pages from the figure at index (buf[4]), starting with page buf[5]
			g_dimensionstoypad.QueryBlock(buf[4], buf[5], q_result, sequence);
			break;
		}
		case 0xD3: // Write
		{
			// Write 4 bytes to page buf[5] to the figure at index buf[4]
			g_dimensionstoypad.WriteBlock(buf[4], buf[5], std::span<const uint8, 4>{buf.begin() + 6, 4}, q_result, sequence);
			break;
		}
		case 0xD4: // Model
		{
			// Get the model id of the figure at index buf[4]
			g_dimensionstoypad.GetModel(std::span<const uint8, 8>{buf.begin() + 4, 8}, sequence, q_result);
			break;
		}
		case 0xD0: // Tag List
		case 0xE1: // PWD
		case 0xE5: // Active
		case 0xFF: // LEDS Query
		{
			// Further investigation required
			cemuLog_log(LogType::Force, "Unimplemented LD Function: {:x}", command);
			break;
		}
		default:
		{
			cemuLog_log(LogType::Force, "Unknown LD Function: {:x}", command);
			break;
		}
		}

		m_queries.push(q_result);
	}

	uint32 DimensionsUSB::LoadFigure(const std::array<uint8, 0x2D * 0x04>& buf, std::unique_ptr<FileStream> file, uint8 pad, uint8 index)
	{
		std::lock_guard lock(m_dimensionsMutex);

		const uint32 id = GetFigureId(buf);

		DimensionsMini& figure = GetFigureByIndex(index);
		figure.dimFile = std::move(file);
		figure.id = id;
		figure.pad = pad;
		figure.index = index + 1;
		figure.data = buf;
		// When a figure is added to the toypad, respond to the game with the pad they were added to, their index,
		// the direction (0x00 in byte 6 for added) and their UID
		std::array<uint8, 32> figureChangeResponse = {0x56, 0x0b, figure.pad, 0x00, figure.index, 0x00, buf[0], buf[1], buf[2], buf[4], buf[5], buf[6], buf[7]};
		figureChangeResponse[13] = GenerateChecksum(figureChangeResponse, 13);
		m_figureAddedRemovedResponses.push(figureChangeResponse);

		return id;
	}

	bool DimensionsUSB::RemoveFigure(uint8 pad, uint8 index, bool fullRemove)
	{
		std::lock_guard lock(m_dimensionsMutex);

		DimensionsMini& figure = GetFigureByIndex(index);
		if (figure.index == 255)
			return false;

		// When a figure is removed from the toypad, respond to the game with the pad they were removed from, their index,
		// the direction (0x01 in byte 6 for removed) and their UID
		if (fullRemove)
		{
			std::array<uint8, 32> figureChangeResponse = {0x56, 0x0b, figure.pad, 0x00, figure.index, 0x01,
														  figure.data[0], figure.data[1], figure.data[2],
														  figure.data[4], figure.data[5], figure.data[6], figure.data[7]};
			figureChangeResponse[13] = GenerateChecksum(figureChangeResponse, 13);
			m_figureAddedRemovedResponses.push(figureChangeResponse);
			figure.Save();
			figure.dimFile.reset();
		}

		figure.index = 255;
		figure.pad = 255;
		figure.id = 0;

		return true;
	}

	bool DimensionsUSB::TempRemove(uint8 index)
	{
		std::lock_guard lock(m_dimensionsMutex);

		DimensionsMini& figure = GetFigureByIndex(index);
		if (figure.index == 255)
			return false;

		// Send a response to the game that the figure has been "Picked up" from existing slot,
		// until either the movement is cancelled, or user chooses a space to move to
		std::array<uint8, 32> figureChangeResponse = {0x56, 0x0b, figure.pad, 0x00, figure.index, 0x01,
													  figure.data[0], figure.data[1], figure.data[2],
													  figure.data[4], figure.data[5], figure.data[6], figure.data[7]};

		figureChangeResponse[13] = GenerateChecksum(figureChangeResponse, 13);
		m_figureAddedRemovedResponses.push(figureChangeResponse);
		return true;
	}

	bool DimensionsUSB::CancelRemove(uint8 index)
	{
		std::lock_guard lock(m_dimensionsMutex);

		DimensionsMini& figure = GetFigureByIndex(index);
		if (figure.index == 255)
			return false;

		// Cancel the previous movement of the figure
		std::array<uint8, 32> figureChangeResponse = {0x56, 0x0b, figure.pad, 0x00, figure.index, 0x00,
													  figure.data[0], figure.data[1], figure.data[2],
													  figure.data[4], figure.data[5], figure.data[6], figure.data[7]};

		figureChangeResponse[13] = GenerateChecksum(figureChangeResponse, 13);
		m_figureAddedRemovedResponses.push(figureChangeResponse);

		return true;
	}

	bool DimensionsUSB::CreateFigure(fs::path pathName, uint32 id)
	{
		FileStream* dimFile(FileStream::createFile2(pathName));
		if (!dimFile)
			return false;

		std::array<uint8, 0x2D * 0x04> fileData{};
		RandomUID(fileData);
		fileData[3] = id & 0xFF;

		std::array<uint8, 7> uid = {fileData[0], fileData[1], fileData[2], fileData[4], fileData[5], fileData[6], fileData[7]};

		// Only characters are created with their ID encrypted and stored in pages 36 and 37,
		// as well as a password stored in page 43. Blank tags have their information populated
		// by the game when it calls the write_block command.
		if (id != 0)
		{
			const std::array<uint8, 16> figureKey = GenerateFigureKey(fileData);

			std::array<uint8, 8> valueToEncrypt = {uint8(id & 0xFF), uint8((id >> 8) & 0xFF), uint8((id >> 16) & 0xFF), uint8((id >> 24) & 0xFF),
												   uint8(id & 0xFF), uint8((id >> 8) & 0xFF), uint8((id >> 16) & 0xFF), uint8((id >> 24) & 0xFF)};

			std::array<uint8, 8> encrypted = Encrypt(valueToEncrypt, figureKey);

			std::memcpy(&fileData[36 * 4], &encrypted[0], 4);
			std::memcpy(&fileData[37 * 4], &encrypted[4], 4);

			std::memcpy(&fileData[43 * 4], PWDGenerate(fileData).data(), 4);
		}
		else
		{
			// Page 38 is used as verification for blank tags
			fileData[(38 * 4) + 1] = 1;
		}

		if (fileData.size() != dimFile->writeData(fileData.data(), fileData.size()))
		{
			delete dimFile;
			return false;
		}
		delete dimFile;
		return true;
	}

	bool DimensionsUSB::MoveFigure(uint8 pad, uint8 index, uint8 oldPad, uint8 oldIndex)
	{
		if (oldIndex == index)
		{
			// Don't bother removing and loading again, just send response to the game
			CancelRemove(index);
			return true;
		}

		// When moving figures between spaces on the toypad, remove any figure from the space they are moving to,
		// then remove them from their current space, then load them to the space they are moving to
		RemoveFigure(pad, index, true);

		DimensionsMini& figure = GetFigureByIndex(oldIndex);
		const std::array<uint8, 0x2D * 0x04> data = figure.data;
		std::unique_ptr<FileStream> inFile = std::move(figure.dimFile);

		RemoveFigure(oldPad, oldIndex, false);

		LoadFigure(data, std::move(inFile), pad, index);

		return true;
	}

	void DimensionsUSB::GenerateRandomNumber(std::span<const uint8, 8> buf, uint8 sequence,
											 std::array<uint8, 32>& replyBuf)
	{
		// Decrypt payload into an 8 byte array
		std::array<uint8, 8> value = Decrypt(buf, std::nullopt);
		// Seed is the first 4 bytes (little endian) of the decrypted payload
		uint32 seed = (uint32&)value[0];
		// Confirmation is the second 4 bytes (big endian) of the decrypted payload
		uint32 conf = (uint32be&)value[4];
		// Initialize rng using the seed from decrypted payload
		InitializeRNG(seed);
		// Encrypt 8 bytes, first 4 bytes is the decrypted confirmation from payload, 2nd 4 bytes are blank
		std::array<uint8, 8> valueToEncrypt = {value[4], value[5], value[6], value[7], 0, 0, 0, 0};
		std::array<uint8, 8> encrypted = Encrypt(valueToEncrypt, std::nullopt);
		replyBuf[0] = 0x55;
		replyBuf[1] = 0x09;
		replyBuf[2] = sequence;
		// Copy encrypted value to response data
		memcpy(&replyBuf[3], encrypted.data(), encrypted.size());
		replyBuf[11] = GenerateChecksum(replyBuf, 11);
	}

	void DimensionsUSB::GetChallengeResponse(std::span<const uint8, 8> buf, uint8 sequence,
											 std::array<uint8, 32>& replyBuf)
	{
		// Decrypt payload into an 8 byte array
		std::array<uint8, 8> value = Decrypt(buf, std::nullopt);
		// Confirmation is the first 4 bytes of the decrypted payload
		uint32 conf = (uint32be&)value[0];
		// Generate next random number based on RNG
		uint32 nextRandom = GetNext();
		// Encrypt an 8 byte array, first 4 bytes are the next random number (little endian)
		// followed by the confirmation from the decrypted payload
		std::array<uint8, 8> valueToEncrypt = {uint8(nextRandom & 0xFF), uint8((nextRandom >> 8) & 0xFF),
											   uint8((nextRandom >> 16) & 0xFF), uint8((nextRandom >> 24) & 0xFF),
											   value[0], value[1], value[2], value[3]};
		std::array<uint8, 8> encrypted = Encrypt(valueToEncrypt, std::nullopt);
		replyBuf[0] = 0x55;
		replyBuf[1] = 0x09;
		replyBuf[2] = sequence;
		// Copy encrypted value to response data
		memcpy(&replyBuf[3], encrypted.data(), encrypted.size());
		replyBuf[11] = GenerateChecksum(replyBuf, 11);

		if (!m_isAwake)
			m_isAwake = true;
	}

	void DimensionsUSB::InitializeRNG(uint32 seed)
	{
		m_randomA = 0xF1EA5EED;
		m_randomB = seed;
		m_randomC = seed;
		m_randomD = seed;

		for (int i = 0; i < 42; i++)
		{
			GetNext();
		}
	}

	uint32 DimensionsUSB::GetNext()
	{
		uint32 e = m_randomA - std::rotl(m_randomB, 21);
		m_randomA = m_randomB ^ std::rotl(m_randomC, 19);
		m_randomB = m_randomC + std::rotl(m_randomD, 6);
		m_randomC = m_randomD + e;
		m_randomD = e + m_randomA;
		return m_randomD;
	}

	std::array<uint8, 8> DimensionsUSB::Decrypt(std::span<const uint8, 8> buf, std::optional<std::array<uint8, 16>> key)
	{
		// Value to decrypt is separated in to two little endian 32 bit unsigned integers
		uint32 dataOne = (uint32&)buf[0];
		uint32 dataTwo = (uint32&)buf[4];

		// Use the key as 4 32 bit little endian unsigned integers
		uint32 keyOne;
		uint32 keyTwo;
		uint32 keyThree;
		uint32 keyFour;

		if (key)
		{
			keyOne = (uint32&)key.value()[0];
			keyTwo = (uint32&)key.value()[4];
			keyThree = (uint32&)key.value()[8];
			keyFour = (uint32&)key.value()[12];
		}
		else
		{
			keyOne = (uint32&)COMMAND_KEY[0];
			keyTwo = (uint32&)COMMAND_KEY[4];
			keyThree = (uint32&)COMMAND_KEY[8];
			keyFour = (uint32&)COMMAND_KEY[12];
		}

		uint32 sum = 0xC6EF3720;
		uint32 delta = 0x9E3779B9;

		for (int i = 0; i < 32; i++)
		{
			dataTwo -= (((dataOne << 4) + keyThree) ^ (dataOne + sum) ^ ((dataOne >> 5) + keyFour));
			dataOne -= (((dataTwo << 4) + keyOne) ^ (dataTwo + sum) ^ ((dataTwo >> 5) + keyTwo));
			sum -= delta;
		}

		cemu_assert(sum == 0);

		std::array<uint8, 8> decrypted = {uint8(dataOne & 0xFF), uint8((dataOne >> 8) & 0xFF),
										  uint8((dataOne >> 16) & 0xFF), uint8((dataOne >> 24) & 0xFF),
										  uint8(dataTwo & 0xFF), uint8((dataTwo >> 8) & 0xFF),
										  uint8((dataTwo >> 16) & 0xFF), uint8((dataTwo >> 24) & 0xFF)};
		return decrypted;
	}
	std::array<uint8, 8> DimensionsUSB::Encrypt(std::span<const uint8, 8> buf, std::optional<std::array<uint8, 16>> key)
	{
		// Value to encrypt is separated in to two little endian 32 bit unsigned integers
		uint32 dataOne = (uint32&)buf[0];
		uint32 dataTwo = (uint32&)buf[4];

		// Use the key as 4 32 bit little endian unsigned integers
		uint32 keyOne;
		uint32 keyTwo;
		uint32 keyThree;
		uint32 keyFour;

		if (key)
		{
			keyOne = (uint32&)key.value()[0];
			keyTwo = (uint32&)key.value()[4];
			keyThree = (uint32&)key.value()[8];
			keyFour = (uint32&)key.value()[12];
		}
		else
		{
			keyOne = (uint32&)COMMAND_KEY[0];
			keyTwo = (uint32&)COMMAND_KEY[4];
			keyThree = (uint32&)COMMAND_KEY[8];
			keyFour = (uint32&)COMMAND_KEY[12];
		}

		uint32 sum = 0;
		uint32 delta = 0x9E3779B9;

		for (int i = 0; i < 32; i++)
		{
			sum += delta;
			dataOne += (((dataTwo << 4) + keyOne) ^ (dataTwo + sum) ^ ((dataTwo >> 5) + keyTwo));
			dataTwo += (((dataOne << 4) + keyThree) ^ (dataOne + sum) ^ ((dataOne >> 5) + keyFour));
		}

		cemu_assert(sum == 0xC6EF3720);

		std::array<uint8, 8> encrypted = {uint8(dataOne & 0xFF), uint8((dataOne >> 8) & 0xFF),
										  uint8((dataOne >> 16) & 0xFF), uint8((dataOne >> 24) & 0xFF),
										  uint8(dataTwo & 0xFF), uint8((dataTwo >> 8) & 0xFF),
										  uint8((dataTwo >> 16) & 0xFF), uint8((dataTwo >> 24) & 0xFF)};
		return encrypted;
	}

	std::array<uint8, 16> DimensionsUSB::GenerateFigureKey(const std::array<uint8, 0x2D * 0x04>& buf)
	{
		std::array<uint8, 7> uid = {buf[0], buf[1], buf[2], buf[4], buf[5], buf[6], buf[7]};

		uint32 scrambleA = Scramble(uid, 3);
		uint32 scrambleB = Scramble(uid, 4);
		uint32 scrambleC = Scramble(uid, 5);
		uint32 scrambleD = Scramble(uid, 6);

		return {uint8((scrambleA >> 24) & 0xFF), uint8((scrambleA >> 16) & 0xFF),
				uint8((scrambleA >> 8) & 0xFF), uint8(scrambleA & 0xFF),
				uint8((scrambleB >> 24) & 0xFF), uint8((scrambleB >> 16) & 0xFF),
				uint8((scrambleB >> 8) & 0xFF), uint8(scrambleB & 0xFF),
				uint8((scrambleC >> 24) & 0xFF), uint8((scrambleC >> 16) & 0xFF),
				uint8((scrambleC >> 8) & 0xFF), uint8(scrambleC & 0xFF),
				uint8((scrambleD >> 24) & 0xFF), uint8((scrambleD >> 16) & 0xFF),
				uint8((scrambleD >> 8) & 0xFF), uint8(scrambleD & 0xFF)};
	}

	uint32 DimensionsUSB::Scramble(const std::array<uint8, 7>& uid, uint8 count)
	{
		std::vector<uint8> toScramble;
		toScramble.reserve(uid.size() + CHAR_CONSTANT.size());
		for (uint8 x : uid)
		{
			toScramble.push_back(x);
		}
		for (uint8 c : CHAR_CONSTANT)
		{
			toScramble.push_back(c);
		}
		toScramble[(count * 4) - 1] = 0xaa;

		std::array<uint8, 4> randomized = DimensionsRandomize(toScramble, count);

		return (uint32be&)randomized[0];
	}

	std::array<uint8, 4> DimensionsUSB::PWDGenerate(const std::array<uint8, 0x2D * 0x04>& buf)
	{
		std::array<uint8, 7> uid = {buf[0], buf[1], buf[2], buf[4], buf[5], buf[6], buf[7]};

		std::vector<uint8> pwdCalc = {PWD_CONSTANT.begin(), PWD_CONSTANT.end() - 1};
		for (uint8 i = 0; i < uid.size(); i++)
		{
			pwdCalc.insert(pwdCalc.begin() + i, uid[i]);
		}

		return DimensionsRandomize(pwdCalc, 8);
	}

	std::array<uint8, 4> DimensionsUSB::DimensionsRandomize(const std::vector<uint8> key, uint8 count)
	{
		uint32 scrambled = 0;
		for (uint8 i = 0; i < count; i++)
		{
			const uint32 v4 = std::rotr(scrambled, 25);
			const uint32 v5 = std::rotr(scrambled, 10);
			const uint32 b = (uint32&)key[i * 4];
			scrambled = b + v4 + v5 - scrambled;
		}
		return {uint8(scrambled & 0xFF), uint8(scrambled >> 8 & 0xFF), uint8(scrambled >> 16 & 0xFF), uint8(scrambled >> 24 & 0xFF)};
	}

	uint32 DimensionsUSB::GetFigureId(const std::array<uint8, 0x2D * 0x04>& buf)
	{
		const std::array<uint8, 16> figureKey = GenerateFigureKey(buf);

		const std::span<const uint8, 8> modelNumber = std::span<const uint8, 8>{buf.begin() + (36 * 4), 8};

		const std::array<uint8, 8> decrypted = Decrypt(modelNumber, figureKey);

		const uint32 figNum = (uint32&)decrypted[0];
		// Characters have their model number encrypted in page 36
		if (figNum < 1000)
		{
			return figNum;
		}
		// Vehicles/Gadgets have their model number written as little endian in page 36
		return (uint32&)modelNumber[0];
	}

	DimensionsUSB::DimensionsMini&
	DimensionsUSB::GetFigureByIndex(uint8 index)
	{
		return m_figures[index];
	}

	void DimensionsUSB::QueryBlock(uint8 index, uint8 page,
								   std::array<uint8, 32>& replyBuf,
								   uint8 sequence)
	{
		std::lock_guard lock(m_dimensionsMutex);

		replyBuf[0] = 0x55;
		replyBuf[1] = 0x12;
		replyBuf[2] = sequence;
		replyBuf[3] = 0x00;

		// Index from game begins at 1 rather than 0, so minus 1 here
		if (const uint8 figureIndex = index - 1; figureIndex < 7)
		{
			const DimensionsMini& figure = GetFigureByIndex(figureIndex);

			// Query 4 pages of 4 bytes from the figure, copy this to the response
			if (figure.index != 255 && (4 * page) < ((0x2D * 4) - 16))
			{
				std::memcpy(&replyBuf[4], figure.data.data() + (4 * page), 16);
			}
		}
		replyBuf[20] = GenerateChecksum(replyBuf, 20);
	}

	void DimensionsUSB::WriteBlock(uint8 index, uint8 page, std::span<const uint8, 4> toWriteBuf,
								   std::array<uint8, 32>& replyBuf, uint8 sequence)
	{
		std::lock_guard lock(m_dimensionsMutex);

		replyBuf[0] = 0x55;
		replyBuf[1] = 0x02;
		replyBuf[2] = sequence;
		replyBuf[3] = 0x00;

		// Index from game begins at 1 rather than 0, so minus 1 here
		if (const uint8 figureIndex = index - 1; figureIndex < 7)
		{
			DimensionsMini& figure = GetFigureByIndex(figureIndex);

			// Copy 4 bytes to the page on the figure requested by the game
			if (figure.index != 255 && page < 0x2D)
			{
				// Id is written to page 36
				if (page == 36)
				{
					figure.id = (uint32&)toWriteBuf[0];
				}
				std::memcpy(figure.data.data() + (page * 4), toWriteBuf.data(), 4);
				figure.Save();
			}
		}
		replyBuf[4] = GenerateChecksum(replyBuf, 4);
	}

	void DimensionsUSB::GetModel(std::span<const uint8, 8> buf, uint8 sequence,
								 std::array<uint8, 32>& replyBuf)
	{
		// Decrypt payload to 8 byte array, byte 1 is the index, 4-7 are the confirmation
		std::array<uint8, 8> value = Decrypt(buf, std::nullopt);
		uint8 index = value[0];
		uint32 conf = (uint32be&)value[4];
		// Response is the figure's id (little endian) followed by the confirmation from payload
		// Index from game begins at 1 rather than 0, so minus 1 here
		std::array<uint8, 8> valueToEncrypt = {};
		if (const uint8 figureIndex = index - 1; figureIndex < 7)
		{
			const DimensionsMini& figure = GetFigureByIndex(figureIndex);
			valueToEncrypt = {uint8(figure.id & 0xFF), uint8((figure.id >> 8) & 0xFF),
							  uint8((figure.id >> 16) & 0xFF), uint8((figure.id >> 24) & 0xFF),
							  value[4], value[5], value[6], value[7]};
		}
		std::array<uint8, 8> encrypted = Encrypt(valueToEncrypt, std::nullopt);
		replyBuf[0] = 0x55;
		replyBuf[1] = 0x0a;
		replyBuf[2] = sequence;
		replyBuf[3] = 0x00;
		memcpy(&replyBuf[4], encrypted.data(), encrypted.size());
		replyBuf[12] = GenerateChecksum(replyBuf, 12);
	}

	void DimensionsUSB::RandomUID(std::array<uint8, 0x2D * 0x04>& uid_buffer)
	{
		uid_buffer[0] = 0x04;
		uid_buffer[7] = 0x80;

		std::random_device rd;
		std::mt19937 mt(rd());
		std::uniform_int_distribution<int> dist(0, 255);

		uid_buffer[1] = dist(mt);
		uid_buffer[2] = dist(mt);
		uid_buffer[4] = dist(mt);
		uid_buffer[5] = dist(mt);
		uid_buffer[6] = dist(mt);
	}

	uint8 DimensionsUSB::GenerateChecksum(const std::array<uint8, 32>& data,
										  int num_of_bytes) const
	{
		int checksum = 0;
		for (int i = 0; i < num_of_bytes; i++)
		{
			checksum += data[i];
		}
		return (checksum & 0xFF);
	}

	void DimensionsUSB::DimensionsMini::Save()
	{
		if (!dimFile)
			return;

		dimFile->SetPosition(0);
		dimFile->writeData(data.data(), data.size());
	}

	std::map<const uint32, const char*> DimensionsUSB::GetListMinifigs()
	{
		return s_listMinis;
	}

	std::map<const uint32, const char*> DimensionsUSB::GetListTokens()
	{
		return s_listTokens;
	}

	std::string DimensionsUSB::FindFigure(uint32 figNum)
	{
		for (const auto& it : GetListMinifigs())
		{
			if (it.first == figNum)
			{
				return it.second;
			}
		}
		for (const auto& it : GetListTokens())
		{
			if (it.first == figNum)
			{
				return it.second;
			}
		}
		return fmt::format("Unknown ({})", figNum);
	}
} // namespace nsyshid