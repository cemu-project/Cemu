#pragma once

namespace nn::temp
{
	void save(MemStreamWriter& s);
	void restore(MemStreamReader& s);

	void Initialize();
};