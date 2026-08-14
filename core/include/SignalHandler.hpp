#ifndef _SIGNAL_HANDLER
#define _SIGNAL_HANDLER

class SignalCallback
{
public:
	SignalCallback() {}
	virtual void suspend() = 0;
	virtual void resume() = 0;
	virtual void shutdown() = 0;
	virtual void alarm() = 0;
	virtual void reset() = 0;
	virtual void childExit() = 0;
	virtual void userdefined1() = 0;
	virtual void userdefined2() = 0;
};

class SignalHandler
{
public:
	SignalHandler();
	~SignalHandler();
	void registerCallbackClient(SignalCallback *clientptr);
	void registerSignalHandlers();
	bool isShutdownSignal(const int signum);
};

#endif
