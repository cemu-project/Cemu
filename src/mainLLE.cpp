#include "gui/wxgui.h"
#include "util/crypto/aes128.h"
#include "gui/MainWindow.h"
#include "gui/guiWrapper.h"
#include "Common/FileStream.h"

void CemuCommonInit();

typedef struct  
{
	/* +0x000 */ uint32be magic;
}ppcAncastHeader_t;

void loadEncryptedPPCAncastKernel()
{
	auto kernelData = FileStream::LoadIntoMemory("kernel.img");
	if (!kernelData)
		exit(-1);
	// check header
	ppcAncastHeader_t* ancastHeader = (ppcAncastHeader_t*)kernelData->data();
	if(ancastHeader->magic != (uint32be)0xEFA282D9)
		assert_dbg(); // invalid magic
	memcpy(memory_getPointerFromPhysicalOffset(0x08000000), kernelData->data(), kernelData->size());
}

void loadPPCBootrom()
{
	auto bootromData = FileStream::LoadIntoMemory("bootrom.bin");
	if (!bootromData)
		exit(-1);
	memcpy(memory_getPointerFromPhysicalOffset(0x00000000), bootromData->data(), bootromData->size());
}

void mainEmulatorLLE()
{
	CemuCommonInit();
	// memory init
	memory_initPhysicalLayout();
	
	// start GUI thread
	gui_create();
	// load kernel ancast image
	loadPPCBootrom();
	loadEncryptedPPCAncastKernel();
	
	PPCTimer_waitForInit();
	// begin execution
	PPCCoreLLE_startSingleCoreScheduler(0x00000100);
}

