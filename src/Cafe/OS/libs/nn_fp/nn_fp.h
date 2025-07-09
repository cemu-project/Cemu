#pragma once
namespace nn
{
	namespace fp
	{
		void save(MemStreamWriter& s);
		void restore(MemStreamReader& s);

		void load();
	}
}