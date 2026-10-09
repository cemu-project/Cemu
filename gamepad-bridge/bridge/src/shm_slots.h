#pragma once
// A memfd split into fixed-size frame slots, mapped into this process.
// Ownership of each slot is tracked by the side that created the ring (docs/IPC.md, "Frames").
#include <cstddef>
#include <cstdint>
#include <vector>

namespace drcb {

class ShmSlots
{
public:
	ShmSlots() = default;
	~ShmSlots();
	ShmSlots(const ShmSlots&) = delete;
	ShmSlots& operator=(const ShmSlots&) = delete;

	// Create a new memfd-backed ring. Returns false on failure (logged).
	bool create(const char* name, uint32_t slot_count, uint32_t slot_size);
	// Map a ring received from the peer. Takes ownership of fd. Returns false on failure (logged).
	bool map(int fd, uint32_t slot_count, uint32_t slot_size);
	void reset();

	int fd() const { return m_fd; }
	uint32_t count() const { return m_count; }
	uint32_t slot_size() const { return m_slot_size; }
	uint8_t* slot(uint32_t i) const { return m_base + size_t(i) * m_slot_size; }
	bool valid(uint32_t i) const { return m_base && i < m_count; }

	// Ownership tracking, for the side that hands slots out.
	bool in_use(uint32_t i) const { return m_in_use[i]; }
	void set_in_use(uint32_t i, bool v) { m_in_use[i] = v; }
	int find_free() const; // -1 if none

private:
	int m_fd = -1;
	uint8_t* m_base = nullptr;
	uint32_t m_count = 0;
	uint32_t m_slot_size = 0;
	std::vector<bool> m_in_use;
};

} // namespace drcb
