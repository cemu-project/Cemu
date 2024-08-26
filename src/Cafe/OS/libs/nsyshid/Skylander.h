#pragma once

#include <mutex>

#include "nsyshid.h"
#include "Backend.h"

#include "Common/FileStream.h"

namespace nsyshid
{
	class SkylanderPortalDevice final : public Device {
	  public:
		SkylanderPortalDevice();
		~SkylanderPortalDevice() = default;

		bool Open() override;

		void Close() override;

		bool IsOpened() override;

		ReadResult Read(ReadMessage* message) override;

		WriteResult Write(WriteMessage* message) override;

		bool GetDescriptor(uint8 descType,
						   uint8 descIndex,
						   uint8 lang,
						   uint8* output,
						   uint32 outputMaxLength) override;

		bool SetProtocol(uint8 ifIndex, uint8 protocol) override;

		bool SetReport(ReportMessage* message) override;

	  private:
		bool m_IsOpened;
	};

	constexpr uint16 SKY_BLOCK_COUNT = 0x40;
	constexpr uint16 SKY_BLOCK_SIZE = 0x10;
	constexpr uint16 SKY_FIGURE_SIZE = SKY_BLOCK_COUNT * SKY_BLOCK_SIZE;
	constexpr uint8 MAX_SKYLANDERS = 16;

	class SkylanderUSB {
	  public:
		struct Skylander final
		{
			std::unique_ptr<FileStream> skyFile;
			uint8 status = 0;
			std::queue<uint8> queuedStatus;
			std::array<uint8, SKY_FIGURE_SIZE> data{};
			uint32 lastId = 0;
			void Save();

			enum : uint8
			{
				REMOVED = 0,
				READY = 1,
				REMOVING = 2,
				ADDED = 3
			};
		};

		struct SkylanderLEDColor final
		{
			uint8 red = 0;
			uint8 green = 0;
			uint8 blue = 0;
		};

		void ControlTransfer(uint8* buf, sint32 originalLength);

		void Activate();
		void Deactivate();
		void SetLeds(uint8 side, uint8 r, uint8 g, uint8 b);

		std::array<uint8, 64> GetStatus();
		void QueryBlock(uint8 skyNum, uint8 block, uint8* replyBuf);
		void WriteBlock(uint8 skyNum, uint8 block, const uint8* toWriteBuf,
						uint8* replyBuf);

		uint8 LoadSkylander(uint8* buf, std::unique_ptr<FileStream> file);
		bool RemoveSkylander(uint8 skyNum);
		bool CreateSkylander(fs::path pathName, uint16 skyId, uint16 skyVar);
		uint16 SkylanderCRC16(uint16 initValue, const uint8* buffer, uint32 size);
		static std::map<const std::pair<const uint16, const uint16>, const char*> GetListSkylanders();
		std::string FindSkylander(uint16 skyId, uint16 skyVar);

	  protected:
		std::mutex m_skyMutex;
		std::mutex m_queryMutex;
		std::array<Skylander, MAX_SKYLANDERS> m_skylanders;

	  private:
		std::queue<std::array<uint8, 64>> m_queries;
		bool m_activated = true;
		uint8 m_interruptCounter = 0;
		SkylanderLEDColor m_colorRight = {};
		SkylanderLEDColor m_colorLeft = {};
		SkylanderLEDColor m_colorTrap = {};
	};
	extern SkylanderUSB g_skyportal;
} // namespace nsyshid