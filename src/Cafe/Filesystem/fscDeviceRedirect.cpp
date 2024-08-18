#include "util/helpers/helpers.h"
#include "Cafe/Filesystem/fscDeviceHostFS.h"
#include "FST/fstUtil.h"

struct RedirectEntry
{
	RedirectEntry(const fs::path& dstPath, sint32 priority) : dstPath(dstPath), priority(priority) {}
	fs::path dstPath;
	sint32 priority;
};

FSAFileTree<RedirectEntry> redirectTree;

void fscDeviceRedirect_add(std::string_view virtualSourcePath, size_t fileSize, const fs::path& targetFilePath, sint32 priority)
{
	// check if source already has a redirection
	RedirectEntry* existingEntry;
	if (redirectTree.getFile(virtualSourcePath, existingEntry))
	{
		if (existingEntry->priority >= priority)
			return; // dont replace entries with equal or higher priority
		// unregister existing entry
		redirectTree.removeFile(virtualSourcePath);
		delete existingEntry;
	}
	RedirectEntry* entry = new RedirectEntry(targetFilePath, priority);
	redirectTree.addFile(virtualSourcePath, fileSize, entry);
}

class fscDeviceTypeRedirect : public fscDeviceC
{
	FSCVirtualFile* fscDeviceOpenByPath(std::string_view path, FSC_ACCESS_FLAG accessFlags, void* ctx, sint32* fscStatus) override
	{
		RedirectEntry* redirectionEntry;

		if (HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE) && redirectTree.getFile(path, redirectionEntry))
			return FSCVirtualFile_Host::OpenFile(redirectionEntry->dstPath, accessFlags, *fscStatus);

		FSCVirtualFile* dirIterator;

		if (HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR) && redirectTree.getDirectory(path, dirIterator))
			return dirIterator;

		return nullptr;
	}

public:
	static fscDeviceTypeRedirect& instance()
	{
		static fscDeviceTypeRedirect _instance;
		return _instance;
	}
};

bool _redirectMapped = false;

void fscDeviceRedirect_map()
{
	if (_redirectMapped)
		return;
	fsc_mount("/", "/", &fscDeviceTypeRedirect::instance(), nullptr, FSC_PRIORITY_REDIRECT);
	_redirectMapped = true;
}
