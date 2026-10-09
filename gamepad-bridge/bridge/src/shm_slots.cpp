#include "shm_slots.h"
#include "log.h"
#include "socket_io.h"

#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace drcb {

ShmSlots::~ShmSlots() { reset(); }

void ShmSlots::reset()
{
	if (m_base)
		munmap(m_base, size_t(m_count) * m_slot_size);
	if (m_fd >= 0)
		close(m_fd);
	m_base = nullptr;
	m_fd = -1;
	m_count = m_slot_size = 0;
	m_in_use.clear();
}

bool ShmSlots::create(const char* name, uint32_t slot_count, uint32_t slot_size)
{
	int fd = create_shared_buffer(name, size_t(slot_count) * slot_size);
	return fd >= 0 && map(fd, slot_count, slot_size);
}

bool ShmSlots::map(int fd, uint32_t slot_count, uint32_t slot_size)
{
	reset();
	const size_t total = size_t(slot_count) * slot_size;
	struct stat st;
	if (fstat(fd, &st) < 0 || size_t(st.st_size) < total)
	{
		LOGE("shared buffer is %lld bytes, expected at least %zu", (long long)st.st_size, total);
		close(fd);
		return false;
	}
	void* p = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED)
	{
		LOGE("mmap shared buffer: %s", strerror(errno));
		close(fd);
		return false;
	}
	m_fd = fd;
	m_base = static_cast<uint8_t*>(p);
	m_count = slot_count;
	m_slot_size = slot_size;
	m_in_use.assign(slot_count, false);
	return true;
}

int ShmSlots::find_free() const
{
	for (uint32_t i = 0; i < m_count; i++)
		if (!m_in_use[i])
			return int(i);
	return -1;
}

} // namespace drcb
