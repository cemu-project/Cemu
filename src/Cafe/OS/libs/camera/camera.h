#pragma once

namespace camera
{

	sint32 CAMOpen(sint32 camHandle);
	sint32 CAMClose(sint32 camHandle);

	void load();
};