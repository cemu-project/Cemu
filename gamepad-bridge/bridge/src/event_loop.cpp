#include "event_loop.h"
#include "log.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace drcb {

EventLoop::EventLoop()
{
	m_epfd = epoll_create1(EPOLL_CLOEXEC);
	if (m_epfd < 0)
		LOGE("epoll_create1: %s", strerror(errno));
}

EventLoop::~EventLoop()
{
	if (m_epfd >= 0)
		close(m_epfd);
}

bool EventLoop::add(int fd, std::function<void()> on_readable)
{
	epoll_event ev{};
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
	{
		LOGE("epoll_ctl ADD fd %d: %s", fd, strerror(errno));
		return false;
	}
	m_handlers[fd] = std::move(on_readable);
	return true;
}

void EventLoop::remove(int fd)
{
	epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
	m_handlers.erase(fd);
}

int EventLoop::add_timer(long long period_ns, std::function<void()> on_tick)
{
	int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	if (tfd < 0)
	{
		LOGE("timerfd_create: %s", strerror(errno));
		return -1;
	}
	itimerspec its{};
	its.it_interval.tv_sec = period_ns / 1'000'000'000;
	its.it_interval.tv_nsec = period_ns % 1'000'000'000;
	its.it_value = its.it_interval;
	timerfd_settime(tfd, 0, &its, nullptr);
	add(tfd, [tfd, cb = std::move(on_tick)]() {
		uint64_t expirations;
		if (read(tfd, &expirations, sizeof(expirations)) == sizeof(expirations))
		{
			if (expirations > 1)
				LOGD("timer fd %d: %llu expirations coalesced (loop was late)", tfd, (unsigned long long)expirations);
			cb();
		}
	});
	return tfd;
}

void EventLoop::run()
{
	m_running = true;
	epoll_event events[16];
	while (m_running)
	{
		int n = epoll_wait(m_epfd, events, 16, -1);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			LOGE("epoll_wait: %s", strerror(errno));
			return;
		}
		for (int i = 0; i < n && m_running; i++)
		{
			auto it = m_handlers.find(events[i].data.fd);
			if (it != m_handlers.end())
			{
				auto cb = it->second; // copy: the handler may remove itself
				cb();
			}
		}
	}
}

} // namespace drcb
