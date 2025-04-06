#include "nn_olv_Common.h"
#include "nn_olv_PostTypes.h"
#include "nn_olv_OfflineDB.h"
#include "Cemu/ncrypto/ncrypto.h" // for base64 encoder/decoder
#include "util/helpers/helpers.h"
#include "config/ActiveSettings.h"
#include "Cafe/CafeSystem.h"
#include <pugixml.hpp>
#include <zlib.h>
#include <zarchive/zarchivereader.h>

namespace nn
{
	namespace olv
	{
		std::mutex g_offlineDBMutex;
		bool g_offlineDBInitialized = false;
		ZArchiveReader* g_offlineDBArchive{nullptr};

		void OfflineDB_LazyInit()
		{
			std::scoped_lock _l(g_offlineDBMutex);
			if(g_offlineDBInitialized)
				return;
			// open archive
			g_offlineDBArchive = ZArchiveReader::OpenFromFile(ActiveSettings::GetUserDataPath("resources/miiverse/OfflineDB.zar"));
			if(!g_offlineDBArchive)
				cemuLog_log(LogType::Force, "Offline miiverse posts are not available");
			g_offlineDBInitialized = true;
		}

		void OfflineDB_Shutdown()
		{
			std::scoped_lock _l(g_offlineDBMutex);
			if(!g_offlineDBInitialized)
				return;
			delete g_offlineDBArchive;
			g_offlineDBInitialized = false;
		}

		bool CheckForOfflineDBFile(const char* filePath, uint32* fileSize)
		{
			if(!g_offlineDBArchive)
				return false;
			ZArchiveNodeHandle fileHandle = g_offlineDBArchive->LookUp(filePath);
			if (!g_offlineDBArchive->IsFile(fileHandle))
				return false;
			if(fileSize)
				*fileSize = g_offlineDBArchive->GetFileSize(fileHandle);
			return true;
		}

		bool LoadOfflineDBFile(const char* filePath, std::vector<uint8>& fileData)
		{
			fileData.clear();
			if(!g_offlineDBArchive)
				return false;
			ZArchiveNodeHandle fileHandle = g_offlineDBArchive->LookUp(filePath);
			if (!g_offlineDBArchive->IsFile(fileHandle))
				return false;
			fileData.resize(g_offlineDBArchive->GetFileSize(fileHandle));
			g_offlineDBArchive->ReadFromFile(fileHandle, 0, fileData.size(), fileData.data());
			return true;
		}

		void TryLoadCompressedMemoImage(DownloadedPostData& downloadedPostData)
		{
			const unsigned char tgaHeader_320x120_32BPP[] = {0x0,0x0,0x2,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x40,0x1,0x78,0x0,0x20,0x8};
			std::string memoImageFilename = fmt::format("memo/{}", (char*)downloadedPostData.downloadedDataBase.postId);
			std::vector<uint8> bitmaskCompressedImg;
			if (!LoadOfflineDBFile(memoImageFilename.c_str(), bitmaskCompressedImg))
				return;
			if (bitmaskCompressedImg.size() != (320*120)/8)
				return;
			std::vector<uint8> decompressedImage;
			decompressedImage.resize(sizeof(tgaHeader_320x120_32BPP) + 320 * 120 * 4);
			memcpy(decompressedImage.data(), tgaHeader_320x120_32BPP, sizeof(tgaHeader_320x120_32BPP));
			uint8* pOut = decompressedImage.data() + sizeof(tgaHeader_320x120_32BPP);
			for(int i=0; i<320*120; i++)
			{
				bool isWhite = (bitmaskCompressedImg[i/8] & (1 << (i%8))) != 0;
				if(isWhite)
				{
					pOut[0] = pOut[1] = pOut[2] = pOut[3] = 0xFF;
				}
				else
				{
					pOut[0] = pOut[1] = pOut[2] = 0;
					pOut[3] = 0xFF;
				}
				pOut += 4;
			}
			// store compressed image
			uLongf compressedDestLen = 40960;
			int r = compress((uint8*)downloadedPostData.downloadedDataBase.compressedMemoBody, &compressedDestLen, decompressedImage.data(), decompressedImage.size());
			if( r != Z_OK)
				return;
			downloadedPostData.downloadedDataBase.compressedMemoBodySize = compressedDestLen;
			downloadedPostData.downloadedDataBase.SetFlag(DownloadedDataBase::FLAGS::HAS_BODY_MEMO);
		}

		void CheckForExternalImage(DownloadedPostData& downloadedPostData)
		{
			std::string externalImageFilename = fmt::format("image/{}.jpg", (char*)downloadedPostData.downloadedDataBase.postId);
			uint32 fileSize;
			if (!CheckForOfflineDBFile(externalImageFilename.c_str(), &fileSize))
				return;
			strcpy((char*)downloadedPostData.downloadedDataBase.externalImageDataUrl, externalImageFilename.c_str());
			downloadedPostData.downloadedDataBase.SetFlag(DownloadedDataBase::FLAGS::HAS_EXTERNAL_IMAGE);
			downloadedPostData.downloadedDataBase.externalImageDataSize = fileSize;
		}

		nnResult _Async_OfflineDB_DownloadPostDataListParam_DownloadPostDataList(coreinit::OSEvent* event, DownloadedTopicData* downloadedTopicData, DownloadedPostData* downloadedPostData, uint32be* postCountOut, uint32 maxCount, DownloadPostDataListParam* param)
		{
			stdx::scope_exit _se([&](){coreinit::OSSignalEvent(event);});

			uint64 titleId = CafeSystem::GetForegroundTitleId();

			memset(downloadedTopicData, 0, sizeof(DownloadedTopicData));
			memset(downloadedPostData, 0, sizeof(DownloadedPostData) * maxCount);
			*postCountOut = 0;

			const char* postXmlFilename = nullptr;
			if(titleId == 0x0005000010143400 || titleId == 0x0005000010143500 || titleId == 0x0005000010143600)
				postXmlFilename = "PostList_WindWakerHD.xml";

			if (!postXmlFilename)
				return OLV_RESULT_SUCCESS;

			// load post XML
			std::vector<uint8> xmlData;
			if (!LoadOfflineDBFile(postXmlFilename, xmlData))
				return OLV_RESULT_SUCCESS;
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_buffer(xmlData.data(), xmlData.size());
			if (!result)
				return OLV_RESULT_SUCCESS;
			// collect list of all post xml nodes
			std::vector<pugi::xml_node> postXmlNodes;
			for (pugi::xml_node postNode = doc.child("posts").child("post"); postNode; postNode = postNode.next_sibling("post"))
				postXmlNodes.push_back(postNode);

			// randomly select up to maxCount posts
			srand(GetTickCount());
			uint32 postCount = 0;
			while(!postXmlNodes.empty() && postCount < maxCount)
			{
				uint32 index = rand() % postXmlNodes.size();
				pugi::xml_node& postNode = postXmlNodes[index];

				auto& addedPost = downloadedPostData[postCount];
				memset(&addedPost, 0, sizeof(DownloadedPostData));
				if (!ParseXML_DownloadedPostData(addedPost, postNode) )
					continue;
				TryLoadCompressedMemoImage(addedPost);
				CheckForExternalImage(addedPost);
				postCount++;
				// remove from post list
				postXmlNodes[index] = postXmlNodes.back();
				postXmlNodes.pop_back();
			}
			*postCountOut = postCount;
			return OLV_RESULT_SUCCESS;
		}

		nnResult OfflineDB_DownloadPostDataListParam_DownloadPostDataList(DownloadedTopicData* downloadedTopicData, DownloadedPostData* downloadedPostData, uint32be* postCountOut, uint32 maxCount, DownloadPostDataListParam* param)
		{
			OfflineDB_LazyInit();

			memset(downloadedTopicData, 0, sizeof(DownloadedTopicData));
			downloadedTopicData->communityId = param->communityId;
			*postCountOut = 0;

			if(param->_HasFlag(DownloadPostDataListParam::FLAGS::SELF_ONLY))
				return OLV_RESULT_SUCCESS; // the offlineDB doesn't contain any self posts

			StackAllocator<coreinit::OSEvent> doneEvent;
			coreinit::OSInitEvent(&doneEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_MANUAL);
			auto asyncTask = std::async(std::launch::async, _Async_OfflineDB_DownloadPostDataListParam_DownloadPostDataList, doneEvent.GetPointer(), downloadedTopicData, downloadedPostData, postCountOut, maxCount, param);
			coreinit::OSWaitEvent(&doneEvent);
			nnResult r = asyncTask.get();
			return r;
		}

		nnResult _Async_OfflineDB_DownloadPostDataListParam_DownloadExternalImageData(coreinit::OSEvent* event, DownloadedDataBase* _this, void* imageDataOut, uint32be* imageSizeOut, uint32 maxSize)
		{
			stdx::scope_exit _se([&](){coreinit::OSSignalEvent(event);});

			if (!_this->TestFlags(_this, DownloadedDataBase::FLAGS::HAS_EXTERNAL_IMAGE))
				return OLV_RESULT_MISSING_DATA;

			// not all games may use JPEG files?
			std::string externalImageFilename = fmt::format("image/{}.jpg", (char*)_this->postId);
			std::vector<uint8> jpegData;
			if (!LoadOfflineDBFile(externalImageFilename.c_str(), jpegData))
				return OLV_RESULT_FAILED_REQUEST;

			memcpy(imageDataOut, jpegData.data(), jpegData.size());
			*imageSizeOut = jpegData.size();

			return OLV_RESULT_SUCCESS;
		}

		nnResult OfflineDB_DownloadPostDataListParam_DownloadExternalImageData(DownloadedDataBase* _this, void* imageDataOut, uint32be* imageSizeOut, uint32 maxSize)
		{
			StackAllocator<coreinit::OSEvent> doneEvent;
			coreinit::OSInitEvent(&doneEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_MANUAL);
			auto asyncTask = std::async(std::launch::async, _Async_OfflineDB_DownloadPostDataListParam_DownloadExternalImageData, doneEvent.GetPointer(), _this, imageDataOut, imageSizeOut, maxSize);
			coreinit::OSWaitEvent(&doneEvent);
			nnResult r = asyncTask.get();
			return r;
		}

	}
}