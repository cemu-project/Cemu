#pragma once
// Single-threaded epoll loop. Every component registers fds with a callback; nothing in the frame
// path hops threads, so there are no hidden queues (CLAUDE.md: flag any buffering in the video path).
#include <functional>
#include <unordered_map>

namespace drcb {

class EventLoop
{
public:
	EventLoop();
	~EventLoop();
	bool ok() const { return m_epfd >= 0; }
	bool add(int fd, std::function<void()> on_readable);
	void remove(int fd);
	void run(); // until stop()
	void stop() { m_running = false; }

	// Periodic timer on its own timerfd. Returns the fd (for remove), -1 on failure.
	int add_timer(long long period_ns, std::function<void()> on_tick);

private:
	int m_epfd = -1;
	bool m_running = false;
	std::unordered_map<int, std::function<void()>> m_handlers;
};

} // namespace drcb
