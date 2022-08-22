#include "prudp.h"
#include "nex.h"
#include "nexThread.h"

std::mutex mtx_queuedServices;
std::vector<nexService*> list_queuedServices;

std::vector<nexService*> list_activeNexServices;

void nexThread_run()
{
	while (true)
	{
		// check for new services
		mtx_queuedServices.lock();
		for(auto& it : list_queuedServices)
			list_activeNexServices.push_back(it);
		list_queuedServices.clear();
		mtx_queuedServices.unlock();
		// if service list is empty then pause
		if (list_activeNexServices.empty())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			continue;
		}
		// update
		for (auto& it : list_activeNexServices)
		{
			it->update();
		}
		// delete services marked for destruction
		sint32 idx = 0;
		sint32 listSize = (sint32)list_activeNexServices.size();
		while (idx < listSize)
		{
			if (list_activeNexServices[idx]->isMarkedForDestruction())
			{
				list_activeNexServices[idx]->destroy();
				listSize--;
				list_activeNexServices[idx] = list_activeNexServices[listSize];
			}
			idx++;
		}
		if (listSize != list_activeNexServices.size())
		{
			list_activeNexServices.resize(listSize);
		}
		// idle for short time
		// todo - find a better way to handle this with lower latency. Maybe using select() with loopback socket for interrupting the select on service change
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

bool _nexThreadLaunched = false;

std::thread::id nexThreadId;

void nexThread_init()
{
	if (_nexThreadLaunched)
		return;
	std::thread t(nexThread_run);
	nexThreadId = t.get_id();
	t.detach();
	_nexThreadLaunched = true;
}

void nexThread_registerService(nexService* service)
{
	mtx_queuedServices.lock();
	nexThread_init();
	list_queuedServices.push_back(service);
	mtx_queuedServices.unlock();
}

bool nexThread_isCurrentThread()
{
	if (_nexThreadLaunched == false)
		return false;
	return std::this_thread::get_id() == nexThreadId;
}