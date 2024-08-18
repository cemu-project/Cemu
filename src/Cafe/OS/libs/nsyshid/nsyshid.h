#pragma once

namespace nsyshid
{
	class Backend;

	void AttachBackend(const std::shared_ptr<Backend>& backend);

	void DetachBackend(const std::shared_ptr<Backend>& backend);

	void load();
} // namespace nsyshid
