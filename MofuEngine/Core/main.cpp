
#if _WIN64
#ifndef WIN32_MEAN_AND_LEAN
#define WIN32_MEAN_AND_LEAN
#endif

#include <Windows.h>
#include <crtdbg.h>

extern bool MofuInitialize();
extern void MofuUpdate();
extern void MofuShutdown();

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#if _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
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