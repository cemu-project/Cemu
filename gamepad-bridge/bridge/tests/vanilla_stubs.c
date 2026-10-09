// Stubs so Vanilla's real input packer (third_party/vanilla/lib/gamepad/input.c) runs inside a test.
// send_to_sockaddr captures the packet instead of sending it.
#include <netinet/in.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

unsigned char g_captured[256];
size_t g_captured_size = 0;

void send_to_sockaddr(int fd, const void* data, size_t size, const struct sockaddr_in* a, size_t as)
{
	(void)fd; (void)a; (void)as;
	g_captured_size = size < sizeof(g_captured) ? size : sizeof(g_captured);
	memcpy(g_captured, data, g_captured_size);
}
void create_server_sockaddr(struct sockaddr_in* addr, size_t* size, uint16_t port) { (void)addr; (void)size; (void)port; }
void vanilla_log(const char* f, ...) { (void)f; }
void vanilla_log_no_newline(const char* f, ...) { (void)f; }
