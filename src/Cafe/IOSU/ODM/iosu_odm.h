#pragma once

namespace iosu
{
	namespace odm
	{
		void save(MemStreamWriter& s);
		void restore(MemStreamReader& s);

		void Initialize();
		void Shutdown();
	}
}