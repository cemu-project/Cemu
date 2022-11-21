#include <signal.h>
#include <execinfo.h>
#include <string.h>
#include <string>
#include "config/CemuConfig.h"
#include "util/helpers/StringHelpers.h"

#if BOOST_OS_LINUX
#include "ELFSymbolTable.h"
#endif

#if BOOST_OS_LINUX
void demangleAndPrintBacktrace(char** backtrace, size_t size)
{
	ELFSymbolTable symTable;
	for (char** i = backtrace; i < backtrace + size; i++)
	{
		std::string_view traceLine{*i};

		// basic check to see if the backtrace line matches expected format
		size_t parenthesesOpen = traceLine.find_last_of('(');
		size_t parenthesesClose = traceLine.find_last_of(')');
		size_t offsetPlus = traceLine.find_last_of('+');
		if (!parenthesesOpen || !parenthesesClose || !offsetPlus ||
			 offsetPlus < parenthesesOpen || offsetPlus > parenthesesClose)
		{
			// fall back to default string
			std::cerr << traceLine << std::endl;
			continue;
		}

		// attempt to resolve symbol from regular symbol table if missing from dynamic symbol table
		uint64 newOffset = -1;
		std::string_view symbolName = traceLine.substr(parenthesesOpen+1, offsetPlus-parenthesesOpen-1);
		if (symbolName.empty())
		{
			uint64 symbolOffset = StringHelpers::ToInt64(traceLine.substr(offsetPlus+1,offsetPlus+1-parenthesesClose-1));
			symbolName = symTable.OffsetToSymbol(symbolOffset, newOffset);
		}

		std::cerr << traceLine.substr(0, parenthesesOpen+1);

		std::cerr << boost::core::demangle(symbolName.empty() ? "" : symbolName.data());

		// print relative or existing symbol offset.
		std::cerr << '+';
		if (newOffset != -1)
		{
			std::cerr << std::hex << newOffset;
			std::cerr << traceLine.substr(parenthesesClose) << std::endl;
		}
		else
		{
			std::cerr << traceLine.substr(offsetPlus+1) << std::endl;
		}
	}
}
#endif

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

	void *array[128];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 128);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);

#if BOOST_OS_LINUX
	char** symbol_trace = backtrace_symbols(array, size);

	if (symbol_trace)
	{
		demangleAndPrintBacktrace(symbol_trace, size);
		free(symbol_trace);
	}
	else
	{
		std::cerr << "Failed to read backtrace" << std::endl;
	}
#else
	backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif

	if (GetConfig().crash_dump == CrashDump::Enabled)
	{
		// reset signal handler to default and re-raise signal to dump core
		signal(sig, SIG_DFL);
		raise(sig);
		return;
	}
	// exit process ignoring all issues
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
	action.sa_flags = 0;
	sigfillset(&action.sa_mask); // don't allow signals to be interrupted

	action.sa_handler = handler_SIGINT;
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
