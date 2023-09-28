#pragma once

namespace nn::nfp
{
	void load();
}

void nnNfp_load();
void nnNfp_update();

bool nnNfp_touchNfcTagFromFile(const fs::path& filePath, uint32* nfcError);

#define NFP_STATE_NONE			(0)
#define NFP_STATE_INIT			(1)
#define NFP_STATE_RW_SEARCH		(2)
#define NFP_STATE_RW_ACTIVE		(3)
#define NFP_STATE_RW_DEACTIVE	(4)
#define NFP_STATE_RW_MOUNT		(5)
#define NFP_STATE_UNEXPECTED	(6)
#define NFP_STATE_RW_MOUNT_ROM	(7)

// CEMU NFC error codes
#define NFC_ERROR_NONE					(0)
#define NFC_ERROR_NO_ACCESS				(1)
#define NFC_ERROR_INVALID_FILE_FORMAT	(2)
