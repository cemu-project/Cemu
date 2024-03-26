#include <mutex>

#include "nsyshid.h"
#include "Backend.h"

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

		bool SetProtocol(uint32 ifIndex, uint32 protocol) override;

		bool SetReport(ReportMessage* message) override;

	  private:
		bool m_IsOpened;
	};

	class SkylanderUSB {
	  public:
		struct Skylander final
		{
			std::FILE* sky_file;
			uint8 status = 0;
			std::queue<uint8> queued_status;
			std::array<uint8, 0x40 * 0x10> data{};
			uint32 last_id = 0;
			void Save();

			enum : uint8
			{
				REMOVED = 0,
				READY = 1,
				REMOVING = 2,
				ADDED = 3
			};
		};
		void control_transfer(uint8* buf, sint32 originalLength);

		void activate();
		void deactivate();
		void set_leds(uint8 r, uint8 g, uint8 b);

		std::array<uint8, 64> get_status();
		void query_block(uint8 sky_num, uint8 block, uint8* reply_buf);
		void write_block(uint8 sky_num, uint8 block, const uint8* to_write_buf,
						 uint8* reply_buf);

		uint8 load_skylander(uint8* buf, std::FILE* file);
		bool remove_skylander(uint8 sky_num);

	  protected:
		std::mutex sky_mutex;
		std::array<Skylander, 16> skylanders;

	  private:
		std::queue<std::array<uint8, 64>> m_queries;
		bool m_activated = true;
		uint8 m_interrupt_counter = 0;
		std::mutex m_queryMutex;
	};
	extern SkylanderUSB g_skyportal;
} // namespace nsyshid