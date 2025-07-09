#pragma once

namespace nlibcurl
{
	void save(MemStreamWriter& s);
	void restore(MemStreamReader& s);

	void load();
}