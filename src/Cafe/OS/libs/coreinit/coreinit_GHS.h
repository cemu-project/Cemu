#pragma once

namespace coreinit
{
	void PrepareGHSRuntime();

	void ci_GHS_Save(MemStreamWriter& s);
	void ci_GHS_Restore(MemStreamReader& s);

	void InitializeGHS();
};