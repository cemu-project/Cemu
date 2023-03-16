#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"

#include "Common/GLInclude/GLInclude.h"

#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"

//#define LOG_READBACK_TIME

struct LatteTextureReadbackQueueEntry
{
	HRTick initiateTime;
	uint32 lastUpdateDrawcallIndex;
	LatteTextureView* textureView;
};

std::vector<LatteTextureReadbackQueueEntry> sTextureScheduledReadbacks; // readbacks that have been queued but the actual transfer has not yet been started
std::queue<LatteTextureReadbackInfo*> sTextureActiveReadbackQueue; // readbacks in flight

void LatteTextureReadback_StartTransfer(LatteTextureView* textureView)
{
	cemuLog_log(LogType::TextureReadback, "[TextureReadback-Start] PhysAddr {:08x} Res {}x{} Fmt {} Slice {} Mip {}", textureView->baseTexture->physAddress, textureView->baseTexture->width, textureView->baseTexture->height, textureView->baseTexture->format, textureView->firstSlice, textureView->firstMip);
	HRTick currentTick = HighResolutionTimer().now().getTick();
	// create info entry and store in ordered linked list
	LatteTextureReadbackInfo* readbackInfo = g_renderer->texture_createReadback(textureView);
	sTextureActiveReadbackQueue.push(readbackInfo);
	readbackInfo->StartTransfer();
	readbackInfo->transferStartTime = currentTick;
}

/*
 * Checks for queued transfers and starts them if at least five drawcalls have passed since the last write
 * Called after a draw sequence is completed
 * Returns true if at least one transfer was started
 */
bool LatteTextureReadback_Update(bool forceStart)
{
	bool hasStartedTransfer = false;
	for (size_t i = 0; i < sTextureScheduledReadbacks.size(); i++)
	{
		LatteTextureReadbackQueueEntry& entry = sTextureScheduledReadbacks[i];
		uint32 numElapsedDrawcalls = LatteGPUState.drawCallCounter - entry.lastUpdateDrawcallIndex;
		if (forceStart || numElapsedDrawcalls >= 5)
		{
#ifdef LOG_READBACK_TIME
			double elapsedSecondsSinceInitiate = HighResolutionTimer::getTimeDiff(entry.initiateTime, HighResolutionTimer().now().getTick());
			char initiateElapsedTimeStr[32];
			sprintf(initiateElapsedTimeStr, "%.4lfms", elapsedSecondsSinceInitiate);
			cemuLog_log(LogType::TextureReadback, "[TextureReadback-Update] Starting transfer for {:08x} after {} elapsed drawcalls. Time since initiate: {} Force-start: {}", entry.textureView->baseTexture->physAddress, numElapsedDrawcalls, initiateElapsedTimeStr, forceStart?"yes":"no");
#endif
			LatteTextureReadback_StartTransfer(entry.textureView);
			// remove element
			vectorRemoveByIndex(sTextureScheduledReadbacks, i);
			i--;
			hasStartedTransfer = true;
		}
	}
	return hasStartedTransfer;
}

/*
 * Called when a texture is deleted
 */
void LatteTextureReadback_NotifyTextureDeletion(LatteTexture* texture)
{
	// delete from queue
	for (size_t i = 0; i < sTextureScheduledReadbacks.size(); i++)
	{
		LatteTextureReadbackQueueEntry& entry = sTextureScheduledReadbacks[i];
		if (entry.textureView->baseTexture == texture)
		{
			vectorRemoveByIndex(sTextureScheduledReadbacks, i);
			break;
		}
	}
}

void LatteTextureReadback_Initate(LatteTextureView* textureView)
{
	// currently we don't support readback for resized textures
	if (textureView->baseTexture->overwriteInfo.hasResolutionOverwrite)
	{
		cemuLog_log(LogType::Force, "_initate(): Readback is not supported for textures with modified resolution");
		return;
	}
	// check if texture isn't already queued for transfer
	for (size_t i = 0; i < sTextureScheduledReadbacks.size(); i++)
	{
		LatteTextureReadbackQueueEntry& entry = sTextureScheduledReadbacks[i];
		if (entry.textureView == textureView)
		{
			entry.lastUpdateDrawcallIndex = LatteGPUState.drawCallCounter;
			return;
		}
	}
	// queue
	LatteTextureReadbackQueueEntry queueEntry;
	queueEntry.initiateTime = HighResolutionTimer().now().getTick();
	queueEntry.textureView = textureView;
	queueEntry.lastUpdateDrawcallIndex = LatteGPUState.drawCallCounter;
	sTextureScheduledReadbacks.emplace_back(queueEntry);
}

void LatteTextureReadback_UpdateFinishedTransfers(bool forceFinish)
{
	if (forceFinish)
	{
		// start any delayed transfers
		LatteTextureReadback_Update(true);
	}
	performanceMonitor.gpuTime_waitForAsync.beginMeasuring();
	while (!sTextureActiveReadbackQueue.empty())
	{
		LatteTextureReadbackInfo* readbackInfo = sTextureActiveReadbackQueue.front();
		if (forceFinish)
		{
			if (!readbackInfo->IsFinished())
			{
				readbackInfo->waitStartTime = HighResolutionTimer().now().getTick();
#ifdef LOG_READBACK_TIME
				if (cemuLog_isLoggingEnabled(LogType::TextureReadback))
				{
					double elapsedSecondsTransfer = HighResolutionTimer::getTimeDiff(readbackInfo->transferStartTime, HighResolutionTimer().now().getTick());
					forceLog_printf("[Texture-Readback] Force-finish: %08x Res %4d/%4d TM %d FMT %04x Transfer time so far: %.4lfms", readbackInfo->hostTextureCopy.physAddress, readbackInfo->hostTextureCopy.width, readbackInfo->hostTextureCopy.height, readbackInfo->hostTextureCopy.tileMode, (uint32)readbackInfo->hostTextureCopy.format, elapsedSecondsTransfer * 1000.0);
				}
#endif
				readbackInfo->forceFinish = true;
				readbackInfo->ForceFinish();
				// rerun logic since ->ForceFinish() can recurively call this function and thus modify the queue
				continue;
			}
		}
		else
		{
			if (!readbackInfo->IsFinished())
				break;
			readbackInfo->waitStartTime = HighResolutionTimer().now().getTick();
		}
		// performance testing
#ifdef LOG_READBACK_TIME
		if (cemuLog_isLoggingEnabled(LogType::TextureReadback))
		{
			HRTick currentTick = HighResolutionTimer().now().getTick();
			double elapsedSecondsTransfer = HighResolutionTimer::getTimeDiff(readbackInfo->transferStartTime, currentTick);
			double elapsedSecondsWaiting = HighResolutionTimer::getTimeDiff(readbackInfo->waitStartTime, currentTick);
			forceLog_printf("[Texture-Readback] %08x Res %4d/%4d TM %d FMT %04x ReadbackLatency: %6.3lfms WaitTime: %6.3lfms ForcedWait %s", readbackInfo->hostTextureCopy.physAddress, readbackInfo->hostTextureCopy.width, readbackInfo->hostTextureCopy.height, readbackInfo->hostTextureCopy.tileMode, (uint32)readbackInfo->hostTextureCopy.format, elapsedSecondsTransfer * 1000.0, elapsedSecondsWaiting * 1000.0, readbackInfo->forceFinish ? "yes" : "no");
		}
#endif
		uint8* pixelData = readbackInfo->GetData();
		LatteTextureLoader_writeReadbackTextureToMemory(&readbackInfo->hostTextureCopy, 0, 0, pixelData);
		readbackInfo->ReleaseData();
		// get the original texture if it still exists and invalidate the current data hash
		LatteTextureView* origTexView = LatteTextureViewLookupCache::lookupSlice(readbackInfo->hostTextureCopy.physAddress, readbackInfo->hostTextureCopy.width, readbackInfo->hostTextureCopy.height, readbackInfo->hostTextureCopy.pitch, 0, 0, readbackInfo->hostTextureCopy.format);
		if (origTexView)
			LatteTC_ResetTextureChangeTracker(origTexView->baseTexture, true);
		delete readbackInfo;
		// remove from queue
		cemu_assert_debug(!sTextureActiveReadbackQueue.empty());
		cemu_assert_debug(readbackInfo == sTextureActiveReadbackQueue.front());
		sTextureActiveReadbackQueue.pop();
	}
	performanceMonitor.gpuTime_waitForAsync.endMeasuring();
}
