#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"

#include "Common/GLInclude/GLInclude.h"

#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"

// #define LOG_READBACK_TIME

struct LatteTextureReadbackQueueEntry
{
	uint32 lastUpdateDrawcallIndex;
	LatteTextureView* textureView;
};

std::vector<LatteTextureReadbackQueueEntry> sTextureScheduledReadbacks; // readbacks that have been queued but the actual transfer has not yet been started
std::queue<LatteTextureReadbackInfo*> sTextureActiveReadbackQueue; // readbacks in flight

void LatteTextureReadback_StartTransfer(LatteTextureView* textureView)
{
	cemuLog_log(LogType::TextureReadback, "[TextureReadback-Start] PhysAddr {:08x} Res {}x{} Fmt {} Slice {} Mip {}", textureView->baseTexture->physAddress, textureView->baseTexture->width, textureView->baseTexture->height, textureView->baseTexture->format, textureView->firstSlice, textureView->firstMip);
	// create info entry and store in ordered linked list
	LatteTextureReadbackInfo* readbackInfo = g_renderer->texture_createReadback(textureView);
	sTextureActiveReadbackQueue.push(readbackInfo);
	readbackInfo->StartTransfer();
	//debug_printf("[Tex-Readback] %08x %dx%d TM %d FMT %04x\n", textureView->baseTexture->physAddress, textureView->baseTexture->width, textureView->baseTexture->height, textureView->baseTexture->tileMode, textureView->baseTexture->format);
	readbackInfo->transferStartTime = HighResolutionTimer().now().getTick();
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
		uint32 numPassedDrawcalls = LatteGPUState.drawCallCounter - entry.lastUpdateDrawcallIndex;
		if (forceStart || numPassedDrawcalls >= 5)
		{
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
		HRTick currentTick = HighResolutionTimer().now().getTick();
		double elapsedSecondsTransfer = HighResolutionTimer::getTimeDiff(readbackInfo->transferStartTime, currentTick);
		double elapsedSecondsWaiting = HighResolutionTimer::getTimeDiff(readbackInfo->waitStartTime, currentTick);
		cemuLog_log(LogType::Force, "[Texture-Readback] {:08x} Res {:4}/{:4} TM {} FMT {:04x} ReadbackLatency: {:6.3}ms WaitTime: {:6.3}ms ForcedWait {}", readbackInfo->hostTextureCopy.physAddress, readbackInfo->hostTextureCopy.width, readbackInfo->hostTextureCopy.height, readbackInfo->hostTextureCopy.tileMode, (uint32)readbackInfo->hostTextureCopy.format, elapsedSecondsTransfer * 1000.0, elapsedSecondsWaiting * 1000.0, forceFinish?"yes":"no");
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
