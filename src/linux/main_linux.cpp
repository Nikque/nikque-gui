// The entry point on Linux.
#include <gtk/gtk.h>

#include <clocale>
#include <csignal>
#include <cstdio>

#include "../app.h"

int main(int argc, char **argv)
{
	// (a write to an engine that has ended fails; it does not end the program)
	std::signal(SIGPIPE, SIG_IGN);
	if (!gtk_init_check(&argc, &argv)) {
		std::fprintf(stderr, "nikque GUI: no display to open the window on\n");
		return 1;
	}
	// (GTK takes the locale of the user; the numbers the program reads and writes keep the point of C)
	std::setlocale(LC_NUMERIC, "C");
	int r;
	{
		ng::App app;
		r = app.run(nullptr, 0);
	}
	return r;
}
