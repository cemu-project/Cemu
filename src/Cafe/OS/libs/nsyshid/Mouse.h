#pragma once

#include "nsyshid.h"
#include "Backend.h"

namespace nsyshid
{
	constexpr uint16 MOUSE_VENDOR_ID = 0x046D;
	constexpr uint16 MOUSE_PRODUCT_ID = 0xC077;

	class MouseDevice final : public Device
	{
	  public:
		MouseDevice();
		~MouseDevice() = default;

		bool Open() override;

		void Close() override;

		bool IsOpened() override;

		ReadResult Read(ReadMessage* message) override;

		WriteResult Write(WriteMessage* message) override;

		bool GetDescriptor(uint8 descType,
						   uint8 descIndex,
						   uint16 lang,
						   uint8* output,
						   uint32 outputMaxLength) override;

		bool SetIdle(uint8 ifIndex,
					 uint8 reportId,
					 uint8 duration) override;

		bool SetProtocol(uint8 ifIndex, uint8 protocol) override;

		bool SetReport(ReportMessage* message) override;

	  private:
		using Report = std::array<uint8, 6>;

		void PollHostMouse();
		Report TakeReport();
		uint32 GetReportLength() const;

		std::mutex m_mutex;
		std::condition_variable m_closeCondition;
		bool m_isOpened = false;
		uint8 m_buttons = 0;
		sint32 m_pendingX = 0;
		sint32 m_pendingY = 0;
		sint32 m_pendingWheel = 0;
		uint8 m_idleDuration = 0;	 // in units of 4ms, 0 means reports are only sent when something changed
		uint8 m_reportProtocol = 1; // 0 = boot protocol, 1 = report protocol
		uint8 m_lastButtons = 0;
		std::chrono::steady_clock::time_point m_lastReportTime{};
	};
} // namespace nsyshid
