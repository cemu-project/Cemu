#include "hid_replay_file.h"
#include "log.h"

#include <cstdio>
#include <cstring>

namespace drcb {

bool load_hid_replay(const std::string& path, std::vector<HidRecord>& out)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
	{
		LOGE("can't open HID replay %s", path.c_str());
		return false;
	}
	char magic[8];
	if (fread(magic, 1, 8, f) != 8 || memcmp(magic, kHidReplayMagic, 8) != 0)
	{
		LOGE("%s is not a HID replay file", path.c_str());
		fclose(f);
		return false;
	}
	HidRecord r;
	out.clear();
	while (fread(&r.t_us, sizeof(r.t_us), 1, f) == 1 && fread(r.packet, 1, 128, f) == 128)
		out.push_back(r);
	fclose(f);
	if (out.empty())
		LOGE("%s has no records", path.c_str());
	return !out.empty();
}

bool save_hid_replay(const std::string& path, const std::vector<HidRecord>& in)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (!f)
		return false;
	bool ok = fwrite(kHidReplayMagic, 1, 8, f) == 8;
	for (const auto& r : in)
		ok = ok && fwrite(&r.t_us, sizeof(r.t_us), 1, f) == 1 && fwrite(r.packet, 1, 128, f) == 128;
	return fclose(f) == 0 && ok;
}

} // namespace drcb
