#include <execinfo.h>
#include <signal.h>

void handler_SIGSEGV(int sig)
{
	printf("SIGSEGV!\n");

	void* array[32];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 32);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	exit(1);
}

void handler_SIGINT(int sig)
{
	/*
	 * Received when pressing CTRL + C in a console
	 * Ideally should be exiting cleanly after saving settings but currently
	 * there's no clean exit pathway (at least on linux) and exiting the app
	 * by any mean ends up with a SIGABRT from the standard library destroying
	 * threads.
	 */
	exit(0);
}

void ExceptionHandler_init()
{
	signal(SIGSEGV, handler_SIGSEGV);
	signal(SIGINT, handler_SIGINT);
}
