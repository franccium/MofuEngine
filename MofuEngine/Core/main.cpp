
#if _WIN64
#ifndef WIN32_MEAN_AND_LEAN
#define WIN32_MEAN_AND_LEAN
#endif


#include <Windows.h>
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

extern bool MofuInitialize();
extern void MofuUpdate();
extern void MofuShutdown();

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF); // NOTE: checks every allocation
#endif
	MSG msg;
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
	MofuShutdown();
	return 0;
}

#endif