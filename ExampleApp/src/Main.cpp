#include "WindowTest.h"
#pragma comment(lib, "mofuengine.lib")

#include <Windows.h>
//NOTE: enable when tracking memory
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

using namespace mofu;

extern bool MofuInitialize();
extern void MofuUpdate();
extern void MofuShutdown();

int WINAPI 
WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#if _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//NOTE: enable when tracking memory
	_CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF);
	//_CrtSetBreakAlloc(658);
#endif

	if (MofuInitialize())
	{
		MSG msg{};
		bool is_running{ true };
		while (is_running)
		{
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				is_running &= (msg.message != WM_QUIT);
			}
			MofuUpdate();
		}
	}
	MofuShutdown();
	return 0;
}