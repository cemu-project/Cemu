#include "WuaConverter.h"

WuaConverter::WuaConverter(const TitleInfo& titleInfo_base, const TitleInfo& titleInfo_update, const TitleInfo& titleInfo_aoc)
	: m_titleInfo_base{titleInfo_base},
	  m_titleInfo_update{titleInfo_update},
	  m_titleInfo_aoc{titleInfo_aoc}
{
}
WuaConverter::~WuaConverter()
{
	m_cancelled = true;
	if (m_workerThread.joinable())
		m_workerThread.join();
}

void WuaConverter::StartConversion(int fd, std::unique_ptr<CompressTitleCallbacks>&& callbacks)
{
	m_workerThread = std::thread([callbacks(std::move(callbacks)), fd, this]() {
		if (fd == -1)
		{
			callbacks->OnError();
			return;
		}

		if (m_started)
			return;

		m_started = true;

		std::vector<TitleInfo*> titlesToConvert;
		if (m_titleInfo_base.IsValid())
			titlesToConvert.emplace_back(&m_titleInfo_base);
		if (m_titleInfo_update.IsValid())
			titlesToConvert.emplace_back(&m_titleInfo_update);
		if (m_titleInfo_aoc.IsValid())
			titlesToConvert.emplace_back(&m_titleInfo_aoc);

		if (titlesToConvert.empty())
		{
			callbacks->OnError();
			return;
		}

		struct ZArchiveWriterContext
		{
			static void NewOutputFile(const int32_t partIndex, void* _ctx)
			{
			}

			static void WriteOutputData(const void* data, size_t length, void* _ctx)
			{
				ZArchiveWriterContext* ctx = (ZArchiveWriterContext*)_ctx;
				auto written = write(ctx->fd, data, length);
				int temp = 10;
			}

			bool RecursivelyCountFiles(const std::string& fscPath)
			{
				sint32 fscStatus;
				std::unique_ptr<FSCVirtualFile> vfDir(fsc_openDirIterator(fscPath.c_str(), &fscStatus));
				if (!vfDir)
					return false;
				if (cancelled)
					return false;
				FSCDirEntry dirEntry;
				while (fsc_nextDir(vfDir.get(), &dirEntry))
				{
					if (dirEntry.isFile)
					{
						totalInputFileSize += (uint64)dirEntry.fileSize;
						totalFileCount++;
					}
					else if (dirEntry.isDirectory)
					{
						if (!RecursivelyCountFiles(fmt::format("{}{}/", fscPath, dirEntry.path)))
						{
							return false;
						}
					}
					else
						cemu_assert_unimplemented();
				}
				return true;
			}

			bool RecursivelyAddFiles(std::string archivePath, std::string fscPath)
			{
				sint32 fscStatus;
				std::unique_ptr<FSCVirtualFile> vfDir(fsc_openDirIterator(fscPath.c_str(), &fscStatus));
				if (!vfDir)
					return false;
				if (cancelled)
					return false;
				zaWriter->MakeDir(archivePath.c_str(), false);
				FSCDirEntry dirEntry;
				while (fsc_nextDir(vfDir.get(), &dirEntry))
				{
					if (dirEntry.isFile)
					{
						zaWriter->StartNewFile((archivePath + dirEntry.path).c_str());
						std::unique_ptr<FSCVirtualFile> vFile(fsc_open((fscPath + dirEntry.path).c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus));
						if (!vFile)
							return false;
						transferBuffer.resize(32 * 1024); // 32KB
						uint32 readBytes;
						while (true)
						{
							readBytes = vFile->fscReadData(transferBuffer.data(), transferBuffer.size());
							if (readBytes == 0)
								break;
							zaWriter->AppendData(transferBuffer.data(), readBytes);
							if (cancelled)
								return false;
							transferredInputBytes += readBytes;
						}
						currentFileIndex++;
					}
					else if (dirEntry.isDirectory)
					{
						if (!RecursivelyAddFiles(fmt::format("{}{}/", archivePath, dirEntry.path), fmt::format("{}{}/", fscPath, dirEntry.path)))
							return false;
					}
					else
						cemu_assert_unimplemented();
				}
				return true;
			}

			bool LoadTitleMetaAndCountFiles(TitleInfo* titleInfo)
			{
				std::string temporaryMountPath = TitleInfo::GetUniqueTempMountingPath();
				titleInfo->Mount(temporaryMountPath, "", FSC_PRIORITY_BASE);
				bool r = RecursivelyCountFiles(temporaryMountPath);
				titleInfo->Unmount(temporaryMountPath);
				return r;
			}

			bool StoreTitle(TitleInfo* titleInfo)
			{
				std::string temporaryMountPath = TitleInfo::GetUniqueTempMountingPath();
				titleInfo->Mount(temporaryMountPath, "", FSC_PRIORITY_BASE);
				bool r = RecursivelyAddFiles(fmt::format("{:016x}_v{}/", titleInfo->GetAppTitleId(), titleInfo->GetAppTitleVersion()), temporaryMountPath);
				titleInfo->Unmount(temporaryMountPath);
				return r;
			}

			bool AddTitles(TitleInfo** titles, size_t count)
			{
				currentFileIndex = 0;
				totalFileCount = 0;
				// count files
				stage = Stage::COLLECTING_FILES;
				for (size_t i = 0; i < count; i++)
				{
					if (!LoadTitleMetaAndCountFiles(titles[i]))
						return false;
					if (cancelled)
						return false;
				}
				// store files
				stage = Stage::COMPRESSING;
				for (size_t i = 0; i < count; i++)
				{
					if (!StoreTitle(titles[i]))
						return false;
				}
				return true;
			}

			~ZArchiveWriterContext()
			{
				delete zaWriter;
			};

			fs::path outputPath;
			int fd;
			ZArchiveWriter* zaWriter{};
			bool isValid{false};
			std::vector<uint8> transferBuffer;
			std::atomic_bool& cancelled;
			// progress
			std::atomic<Stage>& stage;
			std::atomic_uint32_t& totalFileCount;
			std::atomic_uint32_t& currentFileIndex;
			std::atomic_uint64_t& totalInputFileSize;
			std::atomic_uint64_t& transferredInputBytes;
		} writerContext{
			.cancelled = m_cancelled,
			.stage = m_stage,
			.totalFileCount = m_totalFileCount,
			.currentFileIndex = m_currentFileIndex,
			.totalInputFileSize = m_totalInputFileSize,
			.transferredInputBytes = m_transferredInputBytes,
		};

		// mount and store
		writerContext.isValid = true;
		writerContext.fd = fd;
		writerContext.zaWriter = new ZArchiveWriter(&ZArchiveWriterContext::NewOutputFile, &ZArchiveWriterContext::WriteOutputData, &writerContext);
		if (!writerContext.isValid)
		{
			callbacks->OnError();
			return;
		}

		bool result = writerContext.AddTitles(titlesToConvert.data(), titlesToConvert.size());

		if (writerContext.cancelled)
		{
			m_stage = Stage::CANCELLED;
			return;
		}

		m_stage = Stage::FINALIZING;

		if (!result)
		{
			callbacks->OnError();
			return;
		}

		writerContext.zaWriter->Finalize();

		off_t newPos = lseek(fd, 0, SEEK_SET);
		if (newPos == -1)
		{
			callbacks->OnError();
			return;
		}

		boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source> streamBuffer(fd, boost::iostreams::never_close_handle);

		// verify the created WUA file
		ZArchiveReader* zreader = ZArchiveReader::OpenFromStream(std::make_unique<std::istream>(&streamBuffer));
		if (!zreader)
		{
			callbacks->OnError();
			return;
		}
		// todo - do a quick verification here
		delete zreader;

		CafeTitleList::Refresh();

		callbacks->OnFinished();
	});
}

std::string WuaConverter::GetCompressedFileName()
{
	CafeConsoleLanguage languageId = CafeConsoleLanguage::EN; // todo - use user's locale
	std::string shortName;
	if (m_titleInfo_base.IsValid())
		shortName = m_titleInfo_base.GetMetaInfo()->GetShortName(languageId);
	else if (m_titleInfo_update.IsValid())
		shortName = m_titleInfo_update.GetMetaInfo()->GetShortName(languageId);
	else if (m_titleInfo_aoc.IsValid())
		shortName = m_titleInfo_aoc.GetMetaInfo()->GetShortName(languageId);

	if (!shortName.empty())
	{
		boost::replace_all(shortName, ":", "");
	}

	// get the short name, which we will use as a suggested default file name
	std::string defaultFileName = std::move(shortName);
	boost::replace_all(defaultFileName, "/", "");
	boost::replace_all(defaultFileName, "\\", "");

	CafeConsoleRegion region = CafeConsoleRegion::Auto;
	if (m_titleInfo_base.IsValid() && m_titleInfo_base.HasValidXmlInfo())
		region = m_titleInfo_base.GetMetaInfo()->GetRegion();
	else if (m_titleInfo_update.IsValid() && m_titleInfo_update.HasValidXmlInfo())
		region = m_titleInfo_update.GetMetaInfo()->GetRegion();

	if (region == CafeConsoleRegion::JPN)
		defaultFileName.append(" (JP)");
	else if (region == CafeConsoleRegion::EUR)
		defaultFileName.append(" (EU)");
	else if (region == CafeConsoleRegion::USA)
		defaultFileName.append(" (US)");
	if (m_titleInfo_update.IsValid())
	{
		defaultFileName.append(fmt::format(" (v{})", m_titleInfo_update.GetAppTitleVersion()));
	}
	defaultFileName.append(".wua");

	return defaultFileName;
}

std::pair<uint64, uint64> WuaConverter::GetTransferredInputBytes() const
{
	return std::make_pair(m_transferredInputBytes.load(), m_totalInputFileSize.load());
}

Stage WuaConverter::GetCurrentStage() const
{
	return m_stage;
}

uint32 WuaConverter::GetTotalFileCount() const
{
	return m_totalFileCount;
}
