#include "Infinity.h"

#include <random>

#include "nsyshid.h"
#include "Backend.h"

#include "util/crypto/aes128.h"

#include <openssl/crypto.h>
#include "openssl/sha.h"

namespace nsyshid
{
	static constexpr std::array<uint8, 32> SHA1_CONSTANT = {
		0xAF, 0x62, 0xD2, 0xEC, 0x04, 0x91, 0x96, 0x8C, 0xC5, 0x2A, 0x1A, 0x71, 0x65, 0xF8, 0x65, 0xFE,
		0x28, 0x63, 0x29, 0x20, 0x44, 0x69, 0x73, 0x6e, 0x65, 0x79, 0x20, 0x32, 0x30, 0x31, 0x33};

	InfinityUSB g_infinitybase;

	const std::map<const uint32, const std::pair<const uint8, const char*>> s_listFigures = {
		{0x0F4241, {1, "Mr. Incredible"}},
		{0x0F4242, {1, "Sulley"}},
		{0x0F4243, {1, "Jack Sparrow"}},
		{0x0F4244, {1, "Lone Ranger"}},
		{0x0F4245, {1, "Tonto"}},
		{0x0F4246, {1, "Lightning McQueen"}},
		{0x0F4247, {1, "Holley Shiftwell"}},
		{0x0F4248, {1, "Buzz Lightyear"}},
		{0x0F4249, {1, "Jessie"}},
		{0x0F424A, {1, "Mike"}},
		{0x0F424B, {1, "Mrs. Incredible"}},
		{0x0F424C, {1, "Hector Barbossa"}},
		{0x0F424D, {1, "Davy Jones"}},
		{0x0F424E, {1, "Randy"}},
		{0x0F424F, {1, "Syndrome"}},
		{0x0F4250, {1, "Woody"}},
		{0x0F4251, {1, "Mater"}},
		{0x0F4252, {1, "Dash"}},
		{0x0F4253, {1, "Violet"}},
		{0x0F4254, {1, "Francesco Bernoulli"}},
		{0x0F4255, {1, "Sorcerer's Apprentice Mickey"}},
		{0x0F4256, {1, "Jack Skellington"}},
		{0x0F4257, {1, "Rapunzel"}},
		{0x0F4258, {1, "Anna"}},
		{0x0F4259, {1, "Elsa"}},
		{0x0F425A, {1, "Phineas"}},
		{0x0F425B, {1, "Agent P"}},
		{0x0F425C, {1, "Wreck-It Ralph"}},
		{0x0F425D, {1, "Vanellope"}},
		{0x0F425E, {1, "Mr. Incredible (Crystal)"}},
		{0x0F425F, {1, "Jack Sparrow (Crystal)"}},
		{0x0F4260, {1, "Sulley (Crystal)"}},
		{0x0F4261, {1, "Lightning McQueen (Crystal)"}},
		{0x0F4262, {1, "Lone Ranger (Crystal)"}},
		{0x0F4263, {1, "Buzz Lightyear (Crystal)"}},
		{0x0F4264, {1, "Agent P (Crystal)"}},
		{0x0F4265, {1, "Sorcerer's Apprentice Mickey (Crystal)"}},
		{0x0F4266, {1, "Buzz Lightyear (Glowing)"}},
		{0x0F42A4, {2, "Captain America"}},
		{0x0F42A5, {2, "Hulk"}},
		{0x0F42A6, {2, "Iron Man"}},
		{0x0F42A7, {2, "Thor"}},
		{0x0F42A8, {2, "Groot"}},
		{0x0F42A9, {2, "Rocket Raccoon"}},
		{0x0F42AA, {2, "Star-Lord"}},
		{0x0F42AB, {2, "Spider-Man"}},
		{0x0F42AC, {2, "Nick Fury"}},
		{0x0F42AD, {2, "Black Widow"}},
		{0x0F42AE, {2, "Hawkeye"}},
		{0x0F42AF, {2, "Drax"}},
		{0x0F42B0, {2, "Gamora"}},
		{0x0F42B1, {2, "Iron Fist"}},
		{0x0F42B2, {2, "Nova"}},
		{0x0F42B3, {2, "Venom"}},
		{0x0F42B4, {2, "Donald Duck"}},
		{0x0F42B5, {2, "Aladdin"}},
		{0x0F42B6, {2, "Stitch"}},
		{0x0F42B7, {2, "Merida"}},
		{0x0F42B8, {2, "Tinker Bell"}},
		{0x0F42B9, {2, "Maleficent"}},
		{0x0F42BA, {2, "Hiro"}},
		{0x0F42BB, {2, "Baymax"}},
		{0x0F42BC, {2, "Loki"}},
		{0x0F42BD, {2, "Ronan"}},
		{0x0F42BE, {2, "Green Goblin"}},
		{0x0F42BF, {2, "Falcon"}},
		{0x0F42C0, {2, "Yondu"}},
		{0x0F42C1, {2, "Jasmine"}},
		{0x0F42C6, {2, "Black Suit Spider-Man"}},
		{0x0F42D6, {3, "Sam Flynn"}},
		{0x0F42D7, {3, "Quorra"}},
		{0x0F4308, {3, "Anakin Skywalker"}},
		{0x0F4309, {3, "Obi-Wan Kenobi"}},
		{0x0F430A, {3, "Yoda"}},
		{0x0F430B, {3, "Ahsoka Tano"}},
		{0x0F430C, {3, "Darth Maul"}},
		{0x0F430E, {3, "Luke Skywalker"}},
		{0x0F430F, {3, "Han Solo"}},
		{0x0F4310, {3, "Princess Leia"}},
		{0x0F4311, {3, "Chewbacca"}},
		{0x0F4312, {3, "Darth Vader"}},
		{0x0F4313, {3, "Boba Fett"}},
		{0x0F4314, {3, "Ezra Bridger"}},
		{0x0F4315, {3, "Kanan Jarrus"}},
		{0x0F4316, {3, "Sabine Wren"}},
		{0x0F4317, {3, "Zeb Orrelios"}},
		{0x0F4318, {3, "Joy"}},
		{0x0F4319, {3, "Anger"}},
		{0x0F431A, {3, "Fear"}},
		{0x0F431B, {3, "Sadness"}},
		{0x0F431C, {3, "Disgust"}},
		{0x0F431D, {3, "Mickey Mouse"}},
		{0x0F431E, {3, "Minnie Mouse"}},
		{0x0F431F, {3, "Mulan"}},
		{0x0F4320, {3, "Olaf"}},
		{0x0F4321, {3, "Vision"}},
		{0x0F4322, {3, "Ultron"}},
		{0x0F4323, {3, "Ant-Man"}},
		{0x0F4325, {3, "Captain America - The First Avenger"}},
		{0x0F4326, {3, "Finn"}},
		{0x0F4327, {3, "Kylo Ren"}},
		{0x0F4328, {3, "Poe Dameron"}},
		{0x0F4329, {3, "Rey"}},
		{0x0F432B, {3, "Spot"}},
		{0x0F432C, {3, "Nick Wilde"}},
		{0x0F432D, {3, "Judy Hopps"}},
		{0x0F432E, {3, "Hulkbuster"}},
		{0x0F432F, {3, "Anakin Skywalker (Light FX)"}},
		{0x0F4330, {3, "Obi-Wan Kenobi (Light FX)"}},
		{0x0F4331, {3, "Yoda (Light FX)"}},
		{0x0F4332, {3, "Luke Skywalker (Light FX)"}},
		{0x0F4333, {3, "Darth Vader (Light FX)"}},
		{0x0F4334, {3, "Kanan Jarrus (Light FX)"}},
		{0x0F4335, {3, "Kylo Ren (Light FX)"}},
		{0x0F4336, {3, "Black Panther"}},
		{0x0F436C, {3, "Nemo"}},
		{0x0F436D, {3, "Dory"}},
		{0x0F436E, {3, "Baloo"}},
		{0x0F436F, {3, "Alice"}},
		{0x0F4370, {3, "Mad Hatter"}},
		{0x0F4371, {3, "Time"}},
		{0x0F4372, {3, "Peter Pan"}},
		{0x1E8481, {1, "Starter Play Set"}},
		{0x1E8482, {1, "Lone Ranger Play Set"}},
		{0x1E8483, {1, "Cars Play Set"}},
		{0x1E8484, {1, "Toy Story in Space Play Set"}},
		{0x1E84E4, {2, "Marvel's The Avengers Play Set"}},
		{0x1E84E5, {2, "Marvel's Spider-Man Play Set"}},
		{0x1E84E6, {2, "Marvel's Guardians of the Galaxy Play Set"}},
		{0x1E84E7, {2, "Assault on Asgard"}},
		{0x1E84E8, {2, "Escape from the Kyln"}},
		{0x1E84E9, {2, "Stitch's Tropical Rescue"}},
		{0x1E84EA, {2, "Brave Forest Siege"}},
		{0x1E8548, {3, "Inside Out Play Set"}},
		{0x1E8549, {3, "Star Wars: Twilight of the Republic Play Set"}},
		{0x1E854A, {3, "Star Wars: Rise Against the Empire Play Set"}},
		{0x1E854B, {3, "Star Wars: The Force Awakens Play Set"}},
		{0x1E854C, {3, "Marvel Battlegrounds Play Set"}},
		{0x1E854D, {3, "Toy Box Speedway"}},
		{0x1E854E, {3, "Toy Box Takeover"}},
		{0x1E85AC, {3, "Finding Dory Play Set"}},
		{0x2DC6C3, {1, "Bolt's Super Strength"}},
		{0x2DC6C4, {1, "Ralph's Power of Destruction"}},
		{0x2DC6C5, {1, "Chernabog's Power"}},
		{0x2DC6C6, {1, "C.H.R.O.M.E. Damage Increaser"}},
		{0x2DC6C7, {1, "Dr. Doofenshmirtz's Damage-Inator!"}},
		{0x2DC6C8, {1, "Electro-Charge"}},
		{0x2DC6C9, {1, "Fix-It Felix's Repair Power"}},
		{0x2DC6CA, {1, "Rapunzel's Healing"}},
		{0x2DC6CB, {1, "C.H.R.O.M.E. Armor Shield"}},
		{0x2DC6CC, {1, "Star Command Shield"}},
		{0x2DC6CD, {1, "Violet's Force Field"}},
		{0x2DC6CE, {1, "Pieces of Eight"}},
		{0x2DC6CF, {1, "Scrooge McDuck's Lucky Dime"}},
		{0x2DC6D0, {1, "User Control"}},
		{0x2DC6D1, {1, "Sorcerer Mickey's Hat"}},
		{0x2DC6FE, {1, "Emperor Zurg's Wrath"}},
		{0x2DC6FF, {1, "Merlin's Summon"}},
		{0x2DC765, {2, "Enchanted Rose"}},
		{0x2DC766, {2, "Mulan's Training Uniform"}},
		{0x2DC767, {2, "Flubber"}},
		{0x2DC768, {2, "S.H.I.E.L.D. Helicarrier Strike"}},
		{0x2DC769, {2, "Zeus' Thunderbolts"}},
		{0x2DC76A, {2, "King Louie's Monkeys"}},
		{0x2DC76B, {2, "Infinity Gauntlet"}},
		{0x2DC76D, {2, "Sorcerer Supreme"}},
		{0x2DC76E, {2, "Maleficent's Spell Cast"}},
		{0x2DC76F, {2, "Chernabog's Spirit Cyclone"}},
		{0x2DC770, {2, "Marvel Team-Up: Capt. Marvel"}},
		{0x2DC771, {2, "Marvel Team-Up: Iron Patriot"}},
		{0x2DC772, {2, "Marvel Team-Up: Ant-Man"}},
		{0x2DC773, {2, "Marvel Team-Up: White Tiger"}},
		{0x2DC774, {2, "Marvel Team-Up: Yondu"}},
		{0x2DC775, {2, "Marvel Team-Up: Winter Soldier"}},
		{0x2DC776, {2, "Stark Arc Reactor"}},
		{0x2DC777, {2, "Gamma Rays"}},
		{0x2DC778, {2, "Alien Symbiote"}},
		{0x2DC779, {2, "All for One"}},
		{0x2DC77A, {2, "Sandy Claws Surprise"}},
		{0x2DC77B, {2, "Glory Days"}},
		{0x2DC77C, {2, "Cursed Pirate Gold"}},
		{0x2DC77D, {2, "Sentinel of Liberty"}},
		{0x2DC77E, {2, "The Immortal Iron Fist"}},
		{0x2DC77F, {2, "Space Armor"}},
		{0x2DC780, {2, "Rags to Riches"}},
		{0x2DC781, {2, "Ultimate Falcon"}},
		{0x2DC788, {3, "Tomorrowland Time Bomb"}},
		{0x2DC78E, {3, "Galactic Team-Up: Mace Windu"}},
		{0x2DC791, {3, "Luke's Rebel Alliance Flight Suit Costume"}},
		{0x2DC798, {3, "Finn's Stormtrooper Costume"}},
		{0x2DC799, {3, "Poe's Resistance Jacket"}},
		{0x2DC79A, {3, "Resistance Tactical Strike"}},
		{0x2DC79E, {3, "Officer Nick Wilde"}},
		{0x2DC79F, {3, "Meter Maid Judy"}},
		{0x2DC7A2, {3, "Darkhawk's Blast"}},
		{0x2DC7A3, {3, "Cosmic Cube Blast"}},
		{0x2DC7A4, {3, "Princess Leia's Boushh Disguise"}},
		{0x2DC7A6, {3, "Nova Corps Strike"}},
		{0x2DC7A7, {3, "King Mickey"}},
		{0x3D0912, {1, "Mickey's Car"}},
		{0x3D0913, {1, "Cinderella's Coach"}},
		{0x3D0914, {1, "Electric Mayhem Bus"}},
		{0x3D0915, {1, "Cruella De Vil's Car"}},
		{0x3D0916, {1, "Pizza Planet Delivery Truck"}},
		{0x3D0917, {1, "Mike's New Car"}},
		{0x3D0919, {1, "Parking Lot Tram"}},
		{0x3D091A, {1, "Captain Hook's Ship"}},
		{0x3D091B, {1, "Dumbo"}},
		{0x3D091C, {1, "Calico Helicopter"}},
		{0x3D091D, {1, "Maximus"}},
		{0x3D091E, {1, "Angus"}},
		{0x3D091F, {1, "Abu the Elephant"}},
		{0x3D0920, {1, "Headless Horseman's Horse"}},
		{0x3D0921, {1, "Phillipe"}},
		{0x3D0922, {1, "Khan"}},
		{0x3D0923, {1, "Tantor"}},
		{0x3D0924, {1, "Dragon Firework Cannon"}},
		{0x3D0925, {1, "Stitch's Blaster"}},
		{0x3D0926, {1, "Toy Story Mania Blaster"}},
		{0x3D0927, {1, "Flamingo Croquet Mallet"}},
		{0x3D0928, {1, "Carl Fredricksen's Cane"}},
		{0x3D0929, {1, "Hangin' Ten Stitch With Surfboard"}},
		{0x3D092A, {1, "Condorman Glider"}},
		{0x3D092B, {1, "WALL-E's Fire Extinguisher"}},
		{0x3D092C, {1, "On the Grid"}},
		{0x3D092D, {1, "WALL-E's Collection"}},
		{0x3D092E, {1, "King Candy's Dessert Toppings"}},
		{0x3D0930, {1, "Victor's Experiments"}},
		{0x3D0931, {1, "Jack's Scary Decorations"}},
		{0x3D0933, {1, "Frozen Flourish"}},
		{0x3D0934, {1, "Rapunzel's Kingdom"}},
		{0x3D0935, {1, "TRON Interface"}},
		{0x3D0936, {1, "Buy N Large Atmosphere"}},
		{0x3D0937, {1, "Sugar Rush Sky"}},
		{0x3D0939, {1, "New Holland Skyline"}},
		{0x3D093A, {1, "Halloween Town Sky"}},
		{0x3D093C, {1, "Chill in the Air"}},
		{0x3D093D, {1, "Rapunzel's Birthday Sky"}},
		{0x3D0940, {1, "Astro Blasters Space Cruiser"}},
		{0x3D0941, {1, "Marlin's Reef"}},
		{0x3D0942, {1, "Nemo's Seascape"}},
		{0x3D0943, {1, "Alice's Wonderland"}},
		{0x3D0944, {1, "Tulgey Wood"}},
		{0x3D0945, {1, "Tri-State Area Terrain"}},
		{0x3D0946, {1, "Danville Sky"}},
		{0x3D0965, {2, "Stark Tech"}},
		{0x3D0966, {2, "Spider-Streets"}},
		{0x3D0967, {2, "World War Hulk"}},
		{0x3D0968, {2, "Gravity Falls Forest"}},
		{0x3D0969, {2, "Neverland"}},
		{0x3D096A, {2, "Simba's Pridelands"}},
		{0x3D096C, {2, "Calhoun's Command"}},
		{0x3D096D, {2, "Star-Lord's Galaxy"}},
		{0x3D096E, {2, "Dinosaur World"}},
		{0x3D096F, {2, "Groot's Roots"}},
		{0x3D0970, {2, "Mulan's Countryside"}},
		{0x3D0971, {2, "The Sands of Agrabah"}},
		{0x3D0974, {2, "A Small World"}},
		{0x3D0975, {2, "View from the Suit"}},
		{0x3D0976, {2, "Spider-Sky"}},
		{0x3D0977, {2, "World War Hulk Sky"}},
		{0x3D0978, {2, "Gravity Falls Sky"}},
		{0x3D0979, {2, "Second Star to the Right"}},
		{0x3D097A, {2, "The King's Domain"}},
		{0x3D097C, {2, "CyBug Swarm"}},
		{0x3D097D, {2, "The Rip"}},
		{0x3D097E, {2, "Forgotten Skies"}},
		{0x3D097F, {2, "Groot's View"}},
		{0x3D0980, {2, "The Middle Kingdom"}},
		{0x3D0984, {2, "Skies of the World"}},
		{0x3D0985, {2, "S.H.I.E.L.D. Containment Truck"}},
		{0x3D0986, {2, "Main Street Electrical Parade Float"}},
		{0x3D0987, {2, "Mr. Toad's Motorcar"}},
		{0x3D0988, {2, "Le Maximum"}},
		{0x3D0989, {2, "Alice in Wonderland's Caterpillar"}},
		{0x3D098A, {2, "Eglantine's Motorcycle"}},
		{0x3D098B, {2, "Medusa's Swamp Mobile"}},
		{0x3D098C, {2, "Hydra Motorcycle"}},
		{0x3D098D, {2, "Darkwing Duck's Ratcatcher"}},
		{0x3D098F, {2, "The USS Swinetrek"}},
		{0x3D0991, {2, "Spider-Copter"}},
		{0x3D0992, {2, "Aerial Area Rug"}},
		{0x3D0993, {2, "Jack-O-Lantern's Glider"}},
		{0x3D0994, {2, "Spider-Buggy"}},
		{0x3D0995, {2, "Jack Skellington's Reindeer"}},
		{0x3D0996, {2, "Fantasyland Carousel Horse"}},
		{0x3D0997, {2, "Odin's Horse"}},
		{0x3D0998, {2, "Gus the Mule"}},
		{0x3D099A, {2, "Darkwing Duck's Grappling Gun"}},
		{0x3D099C, {2, "Ghost Rider's Chain Whip"}},
		{0x3D099D, {2, "Lew Zealand's Boomerang Fish"}},
		{0x3D099E, {2, "Sergeant Calhoun's Blaster"}},
		{0x3D09A0, {2, "Falcon's Wings"}},
		{0x3D09A1, {2, "Mabel's Kittens for Fists"}},
		{0x3D09A2, {2, "Jim Hawkins' Solar Board"}},
		{0x3D09A3, {2, "Black Panther's Vibranium Knives"}},
		{0x3D09A4, {2, "Cloak of Levitation"}},
		{0x3D09A5, {2, "Aladdin's Magic Carpet"}},
		{0x3D09A6, {2, "Honey Lemon's Ice Capsules"}},
		{0x3D09A7, {2, "Jasmine's Palace View"}},
		{0x3D09C1, {2, "Lola"}},
		{0x3D09C2, {2, "Spider-Cycle"}},
		{0x3D09C3, {2, "The Avenjet"}},
		{0x3D09C4, {2, "Spider-Glider"}},
		{0x3D09C5, {2, "Light Cycle"}},
		{0x3D09C6, {2, "Light Jet"}},
		{0x3D09C9, {3, "Retro Ray Gun"}},
		{0x3D09CA, {3, "Tomorrowland Futurescape"}},
		{0x3D09CB, {3, "Tomorrowland Stratosphere"}},
		{0x3D09CC, {3, "Skies Over Felucia"}},
		{0x3D09CD, {3, "Forests of Felucia"}},
		{0x3D09CF, {3, "General Grievous' Wheel Bike"}},
		{0x3D09D2, {3, "Slave I Flyer"}},
		{0x3D09D3, {3, "Y-Wing Fighter"}},
		{0x3D09D4, {3, "Arlo"}},
		{0x3D09D5, {3, "Nash"}},
		{0x3D09D6, {3, "Butch"}},
		{0x3D09D7, {3, "Ramsey"}},
		{0x3D09DC, {3, "Stars Over Sahara Square"}},
		{0x3D09DD, {3, "Sahara Square Sands"}},
		{0x3D09E0, {3, "Ghost Rider's Motorcycle"}},
		{0x3D09E5, {3, "Quad Jumper"}}};

	InfinityBaseDevice::InfinityBaseDevice()
		: Device(0x0E6F, 0x0129, 1, 2, 0)
	{
		m_IsOpened = false;
	}

	bool InfinityBaseDevice::Open()
	{
		if (!IsOpened())
		{
			m_IsOpened = true;
		}
		return true;
	}

	void InfinityBaseDevice::Close()
	{
		if (IsOpened())
		{
			m_IsOpened = false;
		}
	}

	bool InfinityBaseDevice::IsOpened()
	{
		return m_IsOpened;
	}

	Device::ReadResult InfinityBaseDevice::Read(ReadMessage* message)
	{
		memcpy(message->data, g_infinitybase.GetStatus().data(), message->length);
		message->bytesRead = message->length;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		return Device::ReadResult::Success;
	}

	Device::WriteResult InfinityBaseDevice::Write(WriteMessage* message)
	{
		g_infinitybase.SendCommand(message->data, message->length);
		message->bytesWritten = message->length;
		return Device::WriteResult::Success;
	}

	bool InfinityBaseDevice::GetDescriptor(uint8 descType,
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

	bool InfinityBaseDevice::SetProtocol(uint8 ifIndex, uint8 protocol)
	{
		return true;
	}

	bool InfinityBaseDevice::SetReport(ReportMessage* message)
	{
		return true;
	}

	std::array<uint8, 32> InfinityUSB::GetStatus()
	{
		std::array<uint8, 32> response = {};

		bool responded = false;

		do
		{
			if (!m_figureAddedRemovedResponses.empty())
			{
				memcpy(response.data(), m_figureAddedRemovedResponses.front().data(),
					   0x20);
				m_figureAddedRemovedResponses.pop();
				responded = true;
			}
			else if (!m_queries.empty())
			{
				memcpy(response.data(), m_queries.front().data(), 0x20);
				m_queries.pop();
				responded = true;
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			/* code */
		}
		while (!responded);

		return response;
	}

	void InfinityUSB::SendCommand(uint8* buf, sint32 originalLength)
	{
		const uint8 command = buf[2];
		const uint8 sequence = buf[3];

		std::array<uint8, 32> q_result{};

		switch (command)
		{
		case 0x80:
		{
			q_result = {0xaa, 0x15, 0x00, 0x00, 0x0f, 0x01, 0x00, 0x03,
						0x02, 0x09, 0x09, 0x43, 0x20, 0x32, 0x62, 0x36,
						0x36, 0x4b, 0x34, 0x99, 0x67, 0x31, 0x93, 0x8c};
			break;
		}
		case 0x81:
		{
			// Initiate Challenge
			g_infinitybase.DescrambleAndSeed(buf, sequence, q_result);
			break;
		}
		case 0x83:
		{
			// Challenge Response
			g_infinitybase.GetNextAndScramble(sequence, q_result);
			break;
		}
		case 0x90:
		case 0x92:
		case 0x93:
		case 0x95:
		case 0x96:
		{
			// Color commands
			g_infinitybase.GetBlankResponse(sequence, q_result);
			break;
		}
		case 0xA1:
		{
			// Get Present Figures
			g_infinitybase.GetPresentFigures(sequence, q_result);
			break;
		}
		case 0xA2:
		{
			// Read Block from Figure
			g_infinitybase.QueryBlock(buf[4], buf[5], q_result, sequence);
			break;
		}
		case 0xA3:
		{
			// Write block to figure
			g_infinitybase.WriteBlock(buf[4], buf[5], &buf[7], q_result, sequence);
			break;
		}
		case 0xB4:
		{
			// Get figure ID
			g_infinitybase.GetFigureIdentifier(buf[4], sequence, q_result);
			break;
		}
		case 0xB5:
		{
			// Get status?
			g_infinitybase.GetBlankResponse(sequence, q_result);
			break;
		}
		default:
			cemu_assert_error();
			break;
		}

		m_queries.push(q_result);
	}

	uint8 InfinityUSB::GenerateChecksum(const std::array<uint8, 32>& data,
										int numOfBytes) const
	{
		int checksum = 0;
		for (int i = 0; i < numOfBytes; i++)
		{
			checksum += data[i];
		}
		return (checksum & 0xFF);
	}

	void InfinityUSB::GetBlankResponse(uint8 sequence,
									   std::array<uint8, 32>& replyBuf)
	{
		replyBuf[0] = 0xaa;
		replyBuf[1] = 0x01;
		replyBuf[2] = sequence;
		replyBuf[3] = GenerateChecksum(replyBuf, 3);
	}

	void InfinityUSB::DescrambleAndSeed(uint8* buf, uint8 sequence,
										std::array<uint8, 32>& replyBuf)
	{
		uint64 value = uint64(buf[4]) << 56 | uint64(buf[5]) << 48 |
					   uint64(buf[6]) << 40 | uint64(buf[7]) << 32 |
					   uint64(buf[8]) << 24 | uint64(buf[9]) << 16 |
					   uint64(buf[10]) << 8 | uint64(buf[11]);
		uint32 seed = Descramble(value);
		GenerateSeed(seed);
		GetBlankResponse(sequence, replyBuf);
	}

	void InfinityUSB::GetNextAndScramble(uint8 sequence,
										 std::array<uint8, 32>& replyBuf)
	{
		const uint32 nextRandom = GetNext();
		const uint64 scrambledNextRandom = Scramble(nextRandom, 0);
		replyBuf = {0xAA, 0x09, sequence};
		replyBuf[3] = uint8((scrambledNextRandom >> 56) & 0xFF);
		replyBuf[4] = uint8((scrambledNextRandom >> 48) & 0xFF);
		replyBuf[5] = uint8((scrambledNextRandom >> 40) & 0xFF);
		replyBuf[6] = uint8((scrambledNextRandom >> 32) & 0xFF);
		replyBuf[7] = uint8((scrambledNextRandom >> 24) & 0xFF);
		replyBuf[8] = uint8((scrambledNextRandom >> 16) & 0xFF);
		replyBuf[9] = uint8((scrambledNextRandom >> 8) & 0xFF);
		replyBuf[10] = uint8(scrambledNextRandom & 0xFF);
		replyBuf[11] = GenerateChecksum(replyBuf, 11);
	}

	uint32 InfinityUSB::Descramble(uint64 numToDescramble)
	{
		uint64 mask = 0x8E55AA1B3999E8AA;
		uint32 ret = 0;

		for (int i = 0; i < 64; i++)
		{
			if (mask & 0x8000000000000000)
			{
				ret = (ret << 1) | (numToDescramble & 0x01);
			}

			numToDescramble >>= 1;
			mask <<= 1;
		}

		return ret;
	}

	uint64 InfinityUSB::Scramble(uint32 numToScramble, uint32 garbage)
	{
		uint64 mask = 0x8E55AA1B3999E8AA;
		uint64 ret = 0;

		for (int i = 0; i < 64; i++)
		{
			ret <<= 1;

			if ((mask & 1) != 0)
			{
				ret |= (numToScramble & 1);
				numToScramble >>= 1;
			}
			else
			{
				ret |= (garbage & 1);
				garbage >>= 1;
			}

			mask >>= 1;
		}

		return ret;
	}

	void InfinityUSB::GenerateSeed(uint32 seed)
	{
		m_randomA = 0xF1EA5EED;
		m_randomB = seed;
		m_randomC = seed;
		m_randomD = seed;

		for (int i = 0; i < 23; i++)
		{
			GetNext();
		}
	}

	uint32 InfinityUSB::GetNext()
	{
		uint32 a = m_randomA;
		uint32 b = m_randomB;
		uint32 c = m_randomC;
		uint32 ret = std::rotl(m_randomB, 27);

		const uint32 temp = (a + ((ret ^ 0xFFFFFFFF) + 1));
		b ^= std::rotl(c, 17);
		a = m_randomD;
		c += a;
		ret = b + temp;
		a += temp;

		m_randomC = a;
		m_randomA = b;
		m_randomB = c;
		m_randomD = ret;

		return ret;
	}

	void InfinityUSB::GetPresentFigures(uint8 sequence,
										std::array<uint8, 32>& replyBuf)
	{
		int x = 3;
		for (uint8 i = 0; i < m_figures.size(); i++)
		{
			uint8 slot = i == 0 ? 0x10 : (i < 4) ? 0x20
												 : 0x30;
			if (m_figures[i].present)
			{
				replyBuf[x] = slot + m_figures[i].orderAdded;
				replyBuf[x + 1] = 0x09;
				x += 2;
			}
		}
		replyBuf[0] = 0xaa;
		replyBuf[1] = x - 2;
		replyBuf[2] = sequence;
		replyBuf[x] = GenerateChecksum(replyBuf, x);
	}

	InfinityUSB::InfinityFigure&
	InfinityUSB::GetFigureByOrder(uint8 orderAdded)
	{
		for (uint8 i = 0; i < m_figures.size(); i++)
		{
			if (m_figures[i].orderAdded == orderAdded)
			{
				return m_figures[i];
			}
		}
		return m_figures[0];
	}

	void InfinityUSB::QueryBlock(uint8 fig_num, uint8 block,
								 std::array<uint8, 32>& replyBuf,
								 uint8 sequence)
	{
		std::lock_guard lock(m_infinityMutex);

		InfinityFigure& figure = GetFigureByOrder(fig_num);

		replyBuf[0] = 0xaa;
		replyBuf[1] = 0x12;
		replyBuf[2] = sequence;
		replyBuf[3] = 0x00;
		const uint8 file_block = (block == 0) ? 1 : (block * 4);
		if (figure.present && file_block < 20)
		{
			memcpy(&replyBuf[4], figure.data.data() + (16 * file_block), 16);
		}
		replyBuf[20] = GenerateChecksum(replyBuf, 20);
	}

	void InfinityUSB::WriteBlock(uint8 fig_num, uint8 block,
								 const uint8* to_write_buf,
								 std::array<uint8, 32>& replyBuf,
								 uint8 sequence)
	{
		std::lock_guard lock(m_infinityMutex);

		InfinityFigure& figure = GetFigureByOrder(fig_num);

		replyBuf[0] = 0xaa;
		replyBuf[1] = 0x02;
		replyBuf[2] = sequence;
		replyBuf[3] = 0x00;
		const uint8 file_block = (block == 0) ? 1 : (block * 4);
		if (figure.present && file_block < 20)
		{
			memcpy(figure.data.data() + (file_block * 16), to_write_buf, 16);
			figure.Save();
		}
		replyBuf[4] = GenerateChecksum(replyBuf, 4);
	}

	void InfinityUSB::GetFigureIdentifier(uint8 fig_num, uint8 sequence,
										  std::array<uint8, 32>& replyBuf)
	{
		std::lock_guard lock(m_infinityMutex);

		InfinityFigure& figure = GetFigureByOrder(fig_num);

		replyBuf[0] = 0xaa;
		replyBuf[1] = 0x09;
		replyBuf[2] = sequence;
		replyBuf[3] = 0x00;

		if (figure.present)
		{
			memcpy(&replyBuf[4], figure.data.data(), 7);
		}
		replyBuf[11] = GenerateChecksum(replyBuf, 11);
	}

	std::pair<uint8, std::string> InfinityUSB::FindFigure(uint32 figNum)
	{
		for (const auto& it : GetFigureList())
		{
			if (it.first == figNum)
			{
				return it.second;
			}
		}
		return {0, fmt::format("Unknown Figure ({})", figNum)};
	}

	std::map<const uint32, const std::pair<const uint8, const char*>> InfinityUSB::GetFigureList()
	{
		return s_listFigures;
	}

	void InfinityUSB::InfinityFigure::Save()
	{
		if (!infFile)
			return;

		infFile->SetPosition(0);
		infFile->writeData(data.data(), data.size());
	}

	bool InfinityUSB::RemoveFigure(uint8 position)
	{
		std::lock_guard lock(m_infinityMutex);
		InfinityFigure& figure = m_figures[position];

		figure.Save();
		figure.infFile.reset();

		if (figure.present)
		{
			figure.present = false;

			position = DeriveFigurePosition(position);
			if (position == 0)
			{
				return false;
			}

			std::array<uint8, 32> figureChangeResponse = {0xab, 0x04, position, 0x09, figure.orderAdded,
														  0x01};
			figureChangeResponse[6] = GenerateChecksum(figureChangeResponse, 6);
			m_figureAddedRemovedResponses.push(figureChangeResponse);

			return true;
		}
		return false;
	}

	uint32
	InfinityUSB::LoadFigure(const std::array<uint8, INF_FIGURE_SIZE>& buf,
							std::unique_ptr<FileStream> inFile, uint8 position)
	{
		std::lock_guard lock(m_infinityMutex);
		uint8 orderAdded;

		std::vector<uint8> sha1Calc = {SHA1_CONSTANT.begin(), SHA1_CONSTANT.end() - 1};
		for (int i = 0; i < 7; i++)
		{
			sha1Calc.push_back(buf[i]);
		}

		std::array<uint8, 16> key = GenerateInfinityFigureKey(sha1Calc);

		std::array<uint8, 16> infinity_decrypted_block = {};
		std::array<uint8, 16> encryptedBlock = {};
		memcpy(encryptedBlock.data(), &buf[16], 16);

		AES128_ECB_decrypt(encryptedBlock.data(), key.data(), infinity_decrypted_block.data());

		uint32 number = uint32(infinity_decrypted_block[1]) << 16 | uint32(infinity_decrypted_block[2]) << 8 |
						uint32(infinity_decrypted_block[3]);

		InfinityFigure& figure = m_figures[position];

		figure.infFile = std::move(inFile);
		memcpy(figure.data.data(), buf.data(), figure.data.size());
		figure.present = true;
		if (figure.orderAdded == 255)
		{
			figure.orderAdded = m_figureOrder;
			m_figureOrder++;
		}
		orderAdded = figure.orderAdded;

		position = DeriveFigurePosition(position);
		if (position == 0)
		{
			return 0;
		}

		std::array<uint8, 32> figureChangeResponse = {0xab, 0x04, position, 0x09, orderAdded, 0x00};
		figureChangeResponse[6] = GenerateChecksum(figureChangeResponse, 6);
		m_figureAddedRemovedResponses.push(figureChangeResponse);

		return number;
	}

	static uint32 InfinityCRC32(const std::array<uint8, 16>& buffer)
	{
		static constexpr std::array<uint32, 256> CRC32_TABLE{
			0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535,
			0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd,
			0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d,
			0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
			0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
			0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
			0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac,
			0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
			0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
			0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
			0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb,
			0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
			0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea,
			0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce,
			0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
			0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
			0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409,
			0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
			0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739,
			0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
			0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268,
			0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0,
			0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8,
			0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
			0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
			0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703,
			0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
			0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
			0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae,
			0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
			0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6,
			0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
			0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d,
			0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5,
			0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
			0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
			0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

		// Infinity m_figures calculate their CRC32 based on 12 bytes in the block of 16
		uint32 ret = 0;
		for (uint32 i = 0; i < 12; ++i)
		{
			uint8 index = uint8(ret & 0xFF) ^ buffer[i];
			ret = ((ret >> 8) ^ CRC32_TABLE[index]);
		}

		return ret;
	}

	bool InfinityUSB::CreateFigure(fs::path pathName, uint32 figureNum, uint8 series)
	{
		FileStream* infFile(FileStream::createFile2(pathName));
		if (!infFile)
		{
			return false;
		}
		std::array<uint8, INF_FIGURE_SIZE> fileData{};
		uint32 firstBlock = 0x17878E;
		uint32 otherBlocks = 0x778788;
		for (sint8 i = 2; i >= 0; i--)
		{
			fileData[0x38 - i] = uint8((firstBlock >> i * 8) & 0xFF);
		}
		for (uint32 index = 1; index < 0x05; index++)
		{
			for (sint8 i = 2; i >= 0; i--)
			{
				fileData[((index * 0x40) + 0x38) - i] = uint8((otherBlocks >> i * 8) & 0xFF);
			}
		}
		// Create the vector to calculate the SHA1 hash with
		std::vector<uint8> sha1Calc = {SHA1_CONSTANT.begin(), SHA1_CONSTANT.end() - 1};

		// Generate random UID, used for AES encrypt/decrypt
		std::random_device rd;
		std::mt19937 mt(rd());
		std::uniform_int_distribution<int> dist(0, 255);
		std::array<uint8, 16> uid_data = {0, 0, 0, 0, 0, 0, 0, 0x89, 0x44, 0x00, 0xC2};
		uid_data[0] = dist(mt);
		uid_data[1] = dist(mt);
		uid_data[2] = dist(mt);
		uid_data[3] = dist(mt);
		uid_data[4] = dist(mt);
		uid_data[5] = dist(mt);
		uid_data[6] = dist(mt);
		for (sint8 i = 0; i < 7; i++)
		{
			sha1Calc.push_back(uid_data[i]);
		}
		std::array<uint8, 16> figureData = GenerateBlankFigureData(figureNum, series);
		if (figureData[1] == 0x00)
			return false;

		std::array<uint8, 16> key = GenerateInfinityFigureKey(sha1Calc);

		std::array<uint8, 16> encryptedBlock = {};
		std::array<uint8, 16> blankBlock = {};
		std::array<uint8, 16> encryptedBlank = {};

		AES128_ECB_encrypt(figureData.data(), key.data(), encryptedBlock.data());
		AES128_ECB_encrypt(blankBlock.data(), key.data(), encryptedBlank.data());

		memcpy(&fileData[0], uid_data.data(), uid_data.size());
		memcpy(&fileData[16], encryptedBlock.data(), encryptedBlock.size());
		memcpy(&fileData[16 * 0x04], encryptedBlank.data(), encryptedBlank.size());
		memcpy(&fileData[16 * 0x08], encryptedBlank.data(), encryptedBlank.size());
		memcpy(&fileData[16 * 0x0C], encryptedBlank.data(), encryptedBlank.size());
		memcpy(&fileData[16 * 0x0D], encryptedBlank.data(), encryptedBlank.size());

		infFile->writeData(fileData.data(), fileData.size());

		delete infFile;

		return true;
	}

	std::array<uint8, 16> InfinityUSB::GenerateInfinityFigureKey(const std::vector<uint8>& sha1Data)
	{
		std::array<uint8, 20> digest = {};
		SHA1(sha1Data.data(), sha1Data.size(), digest.data());
		// Infinity AES keys are the first 16 bytes of the SHA1 Digest, every set of 4 bytes need to be
		// reversed due to endianness
		std::array<uint8, 16> key = {};
		for (int i = 0; i < 4; i++)
		{
			for (int x = 3; x >= 0; x--)
			{
				key[(3 - x) + (i * 4)] = digest[x + (i * 4)];
			}
		}
		return key;
	}

	std::array<uint8, 16> InfinityUSB::GenerateBlankFigureData(uint32 figureNum, uint8 series)
	{
		std::array<uint8, 16> figureData = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
											0x00, 0x00, 0x00, 0x01, 0xD1, 0x1F};

		// Figure Number, input by end user
		figureData[1] = uint8((figureNum >> 16) & 0xFF);
		figureData[2] = uint8((figureNum >> 8) & 0xFF);
		figureData[3] = uint8(figureNum & 0xFF);

		// Manufacture date, formatted as YY/MM/DD. Set to release date of figure's series
		if (series == 1)
		{
			figureData[4] = 0x0D;
			figureData[5] = 0x08;
			figureData[6] = 0x12;
		}
		else if (series == 2)
		{
			figureData[4] = 0x0E;
			figureData[5] = 0x09;
			figureData[6] = 0x12;
		}
		else if (series == 3)
		{
			figureData[4] = 0x0F;
			figureData[5] = 0x08;
			figureData[6] = 0x1C;
		}

		uint32 checksum = InfinityCRC32(figureData);
		for (sint8 i = 3; i >= 0; i--)
		{
			figureData[15 - i] = uint8((checksum >> i * 8) & 0xFF);
		}
		return figureData;
	}

	uint8 InfinityUSB::DeriveFigurePosition(uint8 position)
	{
		// In the added/removed response, position needs to be 1 for the hexagon, 2 for Player 1 and
		// Player 1's abilities, and 3 for Player 2 and Player 2's abilities. In the UI, positions 0, 1
		// and 2 represent the hexagon slot, 3, 4 and 5 represent Player 1's slot and 6, 7 and 8 represent
		// Player 2's slot.

		switch (position)
		{
		case 0:
		case 1:
		case 2:
			return 1;
		case 3:
		case 4:
		case 5:
			return 2;
		case 6:
		case 7:
		case 8:
			return 3;

		default:
			return 0;
		}
	}
} // namespace nsyshid