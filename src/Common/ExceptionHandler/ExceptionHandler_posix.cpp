#include <signal.h>
#include <execinfo.h>
#include <string.h>

#include "config/CemuConfig.h"

// handle signals that would dump core, print stacktrace and then dump depending on config
void handlerDumpingSignal(int sig)
{
	char* sigName = strsignal(sig);
	if (sigName)
	{
		printf("%s!\n", sigName);
	}
	else
	{
		// should never be the case
		printf("Unknown core dumping signal!\n");
	}

	void *array[32];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 32);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);

	if (GetConfig().crash_dump == CrashDump::Enabled)
	{
		// reset signal handler to default and re-raise signal to dump core
		signal(sig, SIG_DFL);
		raise(sig);
		return;
	}
	// Exit process ignoring all issues.
	_Exit(1);
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

void ExceptionHandler_init()
{
	struct sigaction action;
	action.sa_handler = handler_SIGINT;
	action.sa_flags = 0;
	sigfillset(&action.sa_mask); // don't allow signals to be interrupted

	sigaction(SIGINT, &action, nullptr);

	action.sa_handler = handlerDumpingSignal;
	sigaction(SIGABRT, &action, nullptr);
	sigaction(SIGBUS, &action, nullptr);
	sigaction(SIGFPE, &action, nullptr);
	sigaction(SIGILL, &action, nullptr);
	sigaction(SIGIOT, &action, nullptr);
	sigaction(SIGQUIT, &action, nullptr);
	sigaction(SIGSEGV, &action, nullptr);
	sigaction(SIGSYS, &action, nullptr);
	sigaction(SIGTRAP, &action, nullptr);
}
