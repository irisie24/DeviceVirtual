#include "SignalHandler.hpp"

#include <signal.h>
#include <stdlib.h>

#ifndef ERESTART
#define ERESTART 999
#endif

namespace
{
int signalNumbers[] = { SIGINT, SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGTERM };

void shutdownCallback(int signo);

SignalCallback* callback = nullptr;

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
}

SignalHandler::SignalHandler()
{
}

SignalHandler::~SignalHandler()
{
}

void SignalHandler::registerCallbackClient(SignalCallback* clientptr)
{
    callback = clientptr;
}

void SignalHandler::registerSignalHandlers()
{
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
}

bool SignalHandler::isShutdownSignal(const int signum)
{
    return isShutdownSignalImpl(signum);
}

namespace
{
void shutdownCallback(int signo)
{
    if (callback && signo != SIGSEGV)
    {
        callback->shutdown();
    }
}
}
