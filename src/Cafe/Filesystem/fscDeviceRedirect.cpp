#include "util/helpers/helpers.h"
#include "Cafe/Filesystem/fscDeviceHostFS.h"
#include "FST/fstUtil.h"

struct RedirectEntry
{
	RedirectEntry(const fs::path& dstPath, sint32 priority) : dstPath(dstPath), priority(priority)
	{
	}
	fs::path dstPath;
	sint32 priority;
};

FileTree<RedirectEntry, false> redirectTree;

void fscDeviceRedirect_add(std::string_view virtualSourcePath, const fs::path& targetFilePath,
						   sint32 priority)
{
	std::wstring virtualSourcePathW = boost::nowide::widen(std::string(virtualSourcePath));
	// check if source already has a redirection
	RedirectEntry* existingEntry;
	if (redirectTree.getFile(virtualSourcePathW, existingEntry))
	{
		if (existingEntry->priority >= priority)
			return; // dont replace entries with equal or higher priority
		// unregister existing entry
		redirectTree.removeFile(virtualSourcePathW.c_str());
		delete existingEntry;
	}
	RedirectEntry* entry = new RedirectEntry(targetFilePath, priority);
	redirectTree.addFile(virtualSourcePathW.c_str(), entry);
}

class fscDeviceTypeRedirect : public fscDeviceC
{
	FSCVirtualFile* fscDeviceOpenByPath(std::wstring_view pathW, FSC_ACCESS_FLAG accessFlags,
										void* ctx, sint32* fscStatus) override
	{
		RedirectEntry* redirectionEntry;
		if (redirectTree.getFile(pathW, redirectionEntry))
			return FSCVirtualFile_Host::OpenFile(redirectionEntry->dstPath, accessFlags,
												 *fscStatus);
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
	fsc_mount("/", L"/", &fscDeviceTypeRedirect::instance(), nullptr, FSC_PRIORITY_REDIRECT);
	_redirectMapped = true;
}
