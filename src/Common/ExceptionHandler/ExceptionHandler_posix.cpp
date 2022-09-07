#include <signal.h>
#include <execinfo.h>
#include <sys/resource.h>

void handler_SIGSEGV(int sig)
{
    printf("SIGSEGV!\n");

    void *array[32];
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
    _Exit(0);
}

// stop large coredumps from clogging up the system disk.
void disableCoreDump()
{
	rlimit l;
	if (!getrlimit(RLIMIT_CORE, &l))
		return;
	l.rlim_cur = 0;
	setrlimit(RLIMIT_CORE, &l);
}

void ExceptionHandler_init()
{
	disableCoreDump();
	signal(SIGSEGV, handler_SIGSEGV);
	signal(SIGINT, handler_SIGINT);
}
