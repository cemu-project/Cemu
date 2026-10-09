#include "socket_io.h"
#include "log.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace drcb {

bool send_msg(int sock, uint16_t type, const void* payload, uint32_t size, int fd_to_pass)
{
	if (sizeof(drcb_header) + size > DRCB_MAX_PACKET)
	{
		LOGE("send_msg: payload too large (type %u, %u bytes)", type, size);
		return false;
	}
	uint8_t buf[DRCB_MAX_PACKET];
	drcb_header h{DRCB_MAGIC, DRCB_VERSION_MAJOR, type, size, 0};
	memcpy(buf, &h, sizeof(h));
	if (size)
		memcpy(buf + sizeof(h), payload, size);

	iovec iov{buf, sizeof(h) + size};
	msghdr msg{};
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))];
	if (fd_to_pass >= 0)
	{
		msg.msg_control = control;
		msg.msg_controllen = sizeof(control);
		cmsghdr* c = CMSG_FIRSTHDR(&msg);
		c->cmsg_level = SOL_SOCKET;
		c->cmsg_type = SCM_RIGHTS;
		c->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(c), &fd_to_pass, sizeof(int));
	}

	ssize_t n;
	do
		n = sendmsg(sock, &msg, MSG_NOSIGNAL);
	while (n < 0 && errno == EINTR);
	if (n < 0)
	{
		// The peer hanging up mid-exchange is normal at disconnect; the reader side reports it.
		if (errno == EPIPE || errno == ECONNRESET)
			LOGD("send_msg: type %u: peer gone", type);
		else
			LOGW("send_msg: type %u failed: %s", type, strerror(errno));
		return false;
	}
	return true;
}

RecvResult recv_msg(int sock, Message& out)
{
	uint8_t buf[DRCB_MAX_PACKET];
	iovec iov{buf, sizeof(buf)};
	alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))];
	msghdr msg{};
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);

	ssize_t n;
	do
		n = recvmsg(sock, &msg, MSG_CMSG_CLOEXEC);
	while (n < 0 && errno == EINTR);
	if (n == 0)
		return RecvResult::Closed;
	if (n < 0)
	{
		if (errno == ECONNRESET)
			return RecvResult::Closed;
		LOGW("recv_msg failed: %s", strerror(errno));
		return RecvResult::Error;
	}

	out.fd = -1;
	for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c))
	{
		if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS)
			memcpy(&out.fd, CMSG_DATA(c), sizeof(int));
	}

	if ((msg.msg_flags & MSG_TRUNC) || size_t(n) < sizeof(drcb_header))
	{
		LOGE("recv_msg: malformed packet (%zd bytes, flags 0x%x)", n, msg.msg_flags);
		if (out.fd >= 0)
			close(out.fd);
		return RecvResult::Error;
	}
	memcpy(&out.header, buf, sizeof(drcb_header));
	if (out.header.magic != DRCB_MAGIC || sizeof(drcb_header) + out.header.payload_size != size_t(n))
	{
		LOGE("recv_msg: bad magic 0x%08x or size %u vs packet %zd", out.header.magic, out.header.payload_size, n);
		if (out.fd >= 0)
			close(out.fd);
		return RecvResult::Error;
	}
	memcpy(out.payload, buf + sizeof(drcb_header), out.header.payload_size);
	return RecvResult::Ok;
}

std::string runtime_dir()
{
	const char* xdg = getenv("XDG_RUNTIME_DIR");
	if (!xdg || !*xdg)
	{
		LOGE("XDG_RUNTIME_DIR is not set; can't place the bridge socket");
		return {};
	}
	std::string dir = std::string(xdg) + "/" + DRCB_SOCKET_DIR;
	if (mkdir(dir.c_str(), 0700) < 0 && errno != EEXIST)
	{
		LOGE("mkdir %s: %s", dir.c_str(), strerror(errno));
		return {};
	}
	struct stat st;
	if (stat(dir.c_str(), &st) < 0 || !S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 0077))
	{
		LOGE("%s must be a directory owned by this user with mode 0700", dir.c_str());
		return {};
	}
	return dir;
}

int create_shared_buffer(const char* name, size_t size)
{
	int fd = memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd < 0)
	{
		LOGE("memfd_create: %s", strerror(errno));
		return -1;
	}
	if (ftruncate(fd, off_t(size)) < 0)
	{
		LOGE("ftruncate memfd to %zu: %s", size, strerror(errno));
		close(fd);
		return -1;
	}
	// The size is part of the contract; stop either side from changing it.
	fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL);
	return fd;
}

} // namespace drcb
