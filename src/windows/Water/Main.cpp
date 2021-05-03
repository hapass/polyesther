#include <windows.h>

LRESULT CALLBACK WaterMainWindowMessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
	WNDCLASS WaterWindowClass = {};
	WaterWindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	WaterWindowClass.lpfnWndProc = WaterMainWindowMessageHandler;
	WaterWindowClass.hInstance = instance;
	WaterWindowClass.lpszClassName = L"WaterMainWindow";

	if (RegisterClass(&WaterWindowClass))
	{
		HWND hWindow = CreateWindowEx(
			0,
			L"WaterMainWindow",
			L"Water", 
			WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
			CW_USEDEFAULT, 
			CW_USEDEFAULT, 
			CW_USEDEFAULT, 
			CW_USEDEFAULT, 
			0, 
			0, 
			instance, 
			0
		);

		if (hWindow)
		{
			MSG message;
			while (GetMessage(&message, 0, 0, 0))
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
		}
	}

	return 0;
}