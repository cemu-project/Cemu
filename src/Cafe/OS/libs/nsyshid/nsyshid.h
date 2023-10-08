#pragma once
namespace nsyshid
{
	void save(MemStreamWriter& s);
	void restore(MemStreamReader& s);

	void load();
}