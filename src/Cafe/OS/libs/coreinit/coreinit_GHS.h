#pragma once

namespace coreinit
{
	void PrepareGHSRuntime();

	void GHS_Save(MemStreamWriter& s);
	void GHS_Restore(MemStreamReader& s);

	void InitializeGHS();
};