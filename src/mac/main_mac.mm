// The entry point on macOS.
#import <Cocoa/Cocoa.h>

#include <csignal>

#include "../app.h"

int main()
{
	// (a write to an engine that has ended fails; it does not end the program)
	std::signal(SIGPIPE, SIG_IGN);
	int r;
	{
		ng::App app;
		r = app.run(nullptr, 0);
	}
	return r;
}
