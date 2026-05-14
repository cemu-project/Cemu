#pragma once

#include "Cafe/TitleList/TitleInfo.h"
#include "Cafe/TitleList/TitleList.h"
#include "JNIUtils.h"
#include "CompressTitleCallbacks.h"

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <zarchive/zarchivewriter.h>
#include <zarchive/zarchivereader.h>

enum class Stage
{
	STARTING,
	CANCELLED,
	COLLECTING_FILES,
	COMPRESSING,
	FINALIZING,
};

class WuaConverter
{
	TitleInfo m_titleInfo_base;
	TitleInfo m_titleInfo_update;
	TitleInfo m_titleInfo_aoc;
	std::atomic_uint64_t m_transferredInputBytes{0};
	std::atomic_uint64_t m_totalInputFileSize{0};
	std::atomic_uint32_t m_currentFileIndex{0};
	std::atomic_uint32_t m_totalFileCount{0};
	std::atomic_bool m_cancelled{false};
	std::atomic<Stage> m_stage{Stage::STARTING};
	std::thread m_workerThread;
	bool m_started{false};

  public:
	WuaConverter(
		const TitleInfo& titleInfo_base,
		const TitleInfo& titleInfo_update,
		const TitleInfo& titleInfo_aoc);

	~WuaConverter();

	std::string GetCompressedFileName();

	std::pair<uint64, uint64> GetTransferredInputBytes() const;
	uint32 GetTotalFileCount() const;
	Stage GetCurrentStage() const;

	void StartConversion(int fd, std::unique_ptr<CompressTitleCallbacks>&& callbacks);
};