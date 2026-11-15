#include <objbase.h>
#include <windows.h>

#include "app.h"

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show)
{
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	int r;
	{
		ng::App app;
		r = app.run(inst, show);
	}
	CoUninitialize();
	return r;
}
