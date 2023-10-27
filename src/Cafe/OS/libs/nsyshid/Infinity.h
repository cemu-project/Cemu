#pragma once

#include <mutex>

#include "nsyshid.h"
#include "Backend.h"

#include "Common/FileStream.h"

namespace nsyshid
{
	class InfinityBaseDevice final : public Device {
	  public:
		InfinityBaseDevice();
		~InfinityBaseDevice() = default;

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

	constexpr uint16 INF_BLOCK_COUNT = 0x14;
	constexpr uint16 INF_BLOCK_SIZE = 0x10;
	constexpr uint16 INF_FIGURE_SIZE = INF_BLOCK_COUNT * INF_BLOCK_SIZE;
	constexpr uint8 MAX_FIGURES = 9;
	class InfinityUSB {
	  public:
		struct InfinityFigure final
		{
			std::unique_ptr<FileStream> infFile;
			std::array<uint8, INF_FIGURE_SIZE> data{};
			bool present = false;
			uint8 orderAdded = 255;
			void Save();
		};

		void SendCommand(uint8* buf, sint32 originalLength);
		std::array<uint8, 32> GetStatus();

		void GetBlankResponse(uint8 sequence, std::array<uint8, 32>& replyBuf);
		void DescrambleAndSeed(uint8* buf, uint8 sequence,
							   std::array<uint8, 32>& replyBuf);
		void GetNextAndScramble(uint8 sequence, std::array<uint8, 32>& replyBuf);
		void GetPresentFigures(uint8 sequence, std::array<uint8, 32>& replyBuf);
		void QueryBlock(uint8 figNum, uint8 block, std::array<uint8, 32>& replyBuf,
						uint8 sequence);
		void WriteBlock(uint8 figNum, uint8 block, const uint8* toWriteBuf,
						std::array<uint8, 32>& replyBuf, uint8 sequence);
		void GetFigureIdentifier(uint8 figNum, uint8 sequence,
								 std::array<uint8, 32>& replyBuf);

		bool RemoveFigure(uint8 position);
		uint32 LoadFigure(const std::array<uint8, INF_FIGURE_SIZE>& buf,
						  std::unique_ptr<FileStream>, uint8 position);
		bool CreateFigure(fs::path pathName, uint32 figureNum, uint8 series);
		static std::map<const uint32, const std::pair<const uint8, const char*>> GetFigureList();
		std::pair<uint8, std::string> FindFigure(uint32 figNum);

	  protected:
		std::shared_mutex m_infinityMutex;
		std::array<InfinityFigure, 9> m_figures;

	  private:
		uint8 GenerateChecksum(const std::array<uint8, 32>& data,
							   int numOfBytes) const;
		uint32 Descramble(uint64 numToDescramble);
		uint64 Scramble(uint32 numToScramble, uint32 garbage);
		void GenerateSeed(uint32 seed);
		uint32 GetNext();
		InfinityFigure& GetFigureByOrder(uint8 orderAdded);
		uint8 DeriveFigurePosition(uint8 position);
		std::array<uint8, 16> GenerateInfinityFigureKey(const std::vector<uint8>& sha1Data);
		std::array<uint8, 16> GenerateBlankFigureData(uint32 figureNum, uint8 series);

		uint32 m_randomA;
		uint32 m_randomB;
		uint32 m_randomC;
		uint32 m_randomD;

		uint8 m_figureOrder = 0;
		std::queue<std::array<uint8, 32>> m_figureAddedRemovedResponses;
		std::queue<std::array<uint8, 32>> m_queries;
	};
	extern InfinityUSB g_infinitybase;

} // namespace nsyshid