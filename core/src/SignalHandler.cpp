#include "SignalHandler.hpp"
#include <signal.h>
#include <stdlib.h>
#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined (WIN64)
#include <memory.h>
#else
#include <string.h>
#include <sys/wait.h>
#endif

// + BSD specific starts
#ifndef SIGSTKFLT
#define SIGSTKFLT 16
#endif
// + BSD specific ends

// + Windows specific starts
#ifndef ERESTART
#define ERESTART 999
#endif
// + Windows specific ends

#if !defined(_WIN32) && !defined(WIN32) && !defined (_WIN64) && !defined (WIN64)
	int signalNumbers[] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGBUS, SIGFPE, SIGSEGV, SIGPIPE, SIGTERM, SIGSTKFLT, SIGUSR1, SIGUSR2, SIGCHLD };
#else
	int signalNumbers[] = { SIGINT, SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGTERM };
#endif

#if defined(WIN32) || defined (_WIN32) || defined (_WIN64) || defined (WIN64)
	static void shutdownCallback(int signo);
#else
	static void alarmCallback(int sig, siginfo_t *siginfo, void *context);
	static void suspendCallback(int sig, siginfo_t *siginfo, void *context);
	static void resumeCallback(int sig, siginfo_t *siginfo, void *context);
	static void shutdownCallback(int sig, siginfo_t *siginfo, void *context);
	static void ignoredCallback(int sig, siginfo_t *siginfo, void *context);
	static void resetCallback(int sig, siginfo_t *siginfo, void *context);
	static void user1Callback(int sig, siginfo_t *siginfo, void *context);
	static void user2Callback(int sig, siginfo_t *siginfo, void *context);
	static void childExitCallback(int sig, siginfo_t *siginfo, void *context);
#endif

SignalCallback *callback = nullptr;

bool isShutdownSignalImpl(const int signum);


SignalHandler::SignalHandler()
{
}

SignalHandler::~SignalHandler()
{
}

void SignalHandler::registerCallbackClient(SignalCallback *clientptr)
{
	callback = clientptr;
}

void SignalHandler::registerSignalHandlers()
{
	#if defined(WIN32) || defined (_WIN32) || defined (_WIN64) || defined (WIN64)
	if (signal(SIGINT, shutdownCallback) == SIG_ERR)
	{
	}

	if (signal(SIGILL, shutdownCallback) == SIG_ERR)
	{
	}

	if (signal(SIGABRT, shutdownCallback) == SIG_ERR)
	{
	}

	if (signal(SIGFPE, shutdownCallback) == SIG_ERR)
	{
	}

	if (signal(SIGSEGV, shutdownCallback) == SIG_ERR)
	{
	}

	if (signal(SIGTERM, shutdownCallback) == SIG_ERR)
	{
	}
	#else
	struct sigaction act;

	for (int signum = 1; signum < 32; signum++)
	{
		memset(&act, '\0', sizeof(act));
		act.sa_flags = SA_SIGINFO;

		switch (signum)
		{
		case SIGKILL:
		case SIGSTOP:
		{
			continue;
		}
		case SIGINT:
		case SIGQUIT:
		case SIGILL:
		case SIGTRAP:
		case SIGABRT:
		case SIGBUS:
		case SIGFPE:
		case SIGSEGV:
		case SIGPIPE:
		case SIGTERM:
		case SIGSTKFLT:
		{
			act.sa_sigaction = &shutdownCallback;
			break;
		}
		case SIGALRM:
		{
			act.sa_sigaction = &alarmCallback;
			break;
		}
		case SIGTSTP:
		{
			act.sa_sigaction = &suspendCallback;
			break;
		}
		case SIGCONT:
		{
			act.sa_sigaction = &resumeCallback;
			break;
		}
		case SIGHUP:
		{
			act.sa_sigaction = &resetCallback;
			break;
		}
		case SIGCHLD:
		{
			act.sa_sigaction = &childExitCallback;
			break;
		}
		case SIGUSR1:
		{
			act.sa_sigaction = &user1Callback;
			break;
		}
		case SIGUSR2:
		{
			act.sa_sigaction = &user2Callback;
			break;
		}
		default:
		{
			act.sa_sigaction = &ignoredCallback;
		}
		}

		if (sigaction(signum, &act, NULL) < 0)
		{
		}
		else
		{
		}
	}
#endif
}

bool SignalHandler::isShutdownSignal(const int signum)
{
	return isShutdownSignalImpl(signum);
}

bool isShutdownSignalImpl(const int signum)
{
	const auto signalCount = sizeof(signalNumbers) / sizeof(signalNumbers[0]);
	for (int ctr = 0; ctr < signalCount; ++ctr)
	{
		if (signalNumbers[ctr] == signum)
		{
			return true;
		}
	}

	return false;
}

#if defined(WIN32) || defined (_WIN32) || defined (_WIN64) || defined (WIN64)
void shutdownCallback(int signo)
{
	if (callback && signo != SIGSEGV)
	{
		callback->shutdown();
	}
}
#else

void suspendCallback(int sig, siginfo_t *siginfo, void *context)
{
	if (callback)
	{
		callback->suspend();
	}
}

void resumeCallback(int sig, siginfo_t *siginfo, void *context)
{
	if (callback)
	{
		callback->resume();
	}
}

void shutdownCallback(int sig, siginfo_t *siginfo, void *context)
{
	if (sig == SIGSEGV)
	{
		exit(1);
	}

	if (callback)
	{
		callback->shutdown();
	}
}

void ignoredCallback(int sig, siginfo_t *siginfo, void *context)
{
	if (isShutdownSignalImpl(sig))
	{
		shutdownCallback(sig, siginfo, context);
		return;
	}
}

void alarmCallback(int sig, siginfo_t *siginfo, void *context)
{
	if (callback)
	{
		callback->alarm();
	}
}

void resetCallback(int sig, siginfo_t *siginfo, void *context)
{
	if (callback)
	{
		callback->reset();
	}
}

void childExitCallback(int sig, siginfo_t *siginfo, void *context)
{
	while (waitpid(-1, NULL, WNOHANG) > 0)
	{
	}

	if (callback)
	{
		callback->childExit();
	}
}

void user1Callback(int sig, siginfo_t *siginfo, void *context)
{
	if (callback)
	{
		callback->userdefined1();
	}
}

void user2Callback(int sig, siginfo_t *siginfo, void *context)
{
	if (callback)
	{
		callback->userdefined2();
	}
}

#endif
