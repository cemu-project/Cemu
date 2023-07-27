#pragma once
#include "Cafe/OS/libs/nn_common.h"
#include "nn_olv_Common.h"

namespace nn
{
	namespace olv
	{
		void OfflineDB_Init();
		void OfflineDB_Shutdown();

		nnResult OfflineDB_DownloadPostDataListParam_DownloadPostDataList(DownloadedTopicData* downloadedTopicData, DownloadedPostData* downloadedPostData, uint32be* postCountOut, uint32 maxCount, DownloadPostDataListParam* param);
		nnResult OfflineDB_DownloadPostDataListParam_DownloadExternalImageData(DownloadedDataBase* _this, void* imageDataOut, uint32be* imageSizeOut, uint32 maxSize);

	}
}