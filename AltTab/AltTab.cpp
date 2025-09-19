#include "framework.h"
#include "AltTab.h"
#include <vector>
#include <string>
#include <dwmapi.h>	
#include <iostream>

#pragma comment(lib, "dwmapi.lib")

#define MAX_LOADSTRING 100
#define HOTKEY_ID 9001

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

struct WindowInfo {
	HWND hWnd;
	std::wstring title;
};

std::vector<WindowInfo> g_openWindows;
int g_selectedIndex = -1; 

HWND g_hSwitcherWnd = NULL;
const WCHAR SWITCHER_CLASS_NAME[] = L"AltTabSwitcherWindowClass";

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SwitcherProc(HWND, UINT, WPARAM, LPARAM); 
BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
void EnumerateVisibleWindows();
void ActivateSelectedWindow();
void ShowSwitcherUI();
void HideSwitcherUI();
void SelectNextWindow();
void SelectPreviousWindow();

void Log(const std::wstring& message) {
	std::wcout << L"[ALTTabReplacer] " << message << std::endl;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int	  nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	#ifdef _DEBUG
		AllocConsole();
		FILE* pCon;
		freopen_s(&pCon, "CONOUT$", "w", stdout);
		freopen_s(&pCon, "CONIN$", "r", stdin);
	#endif

	Log(L"Application Started");

	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_ALTTAB, szWindowClass, MAX_LOADSTRING);

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = szWindowClass;
	if (!RegisterClassExW(&wc)) {
		return FALSE;
	}

	WNDCLASSEXW switcher_wc = {};
	switcher_wc.cbSize = sizeof(WNDCLASSEX);
	switcher_wc.lpfnWndProc = SwitcherProc;
	switcher_wc.hInstance = hInstance;
	switcher_wc.lpszClassName = SWITCHER_CLASS_NAME;
	switcher_wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	if (!RegisterClassExW(&switcher_wc)) {
		Log(L"Main window class registration failed.");
		return FALSE;
	}

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd) {
		Log(L"Switcher window class registration failed.");
		return FALSE; 
	}
	hInst = hInstance;
	
	if (!RegisterHotKey(hWnd, HOTKEY_ID, MOD_ALT | MOD_NOREPEAT, VK_TAB)) {
		Log(L"Main window creation failed.");
		return FALSE;
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterHotKey(hWnd, HOTKEY_ID);
	#ifdef _DEBUG
		FreeConsole(); // Clean up if we allocated it
	#endif
	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_HOTKEY:
		if (wParam == HOTKEY_ID) {
			EnumerateVisibleWindows();
			if (!g_openWindows.empty()) {
				g_selectedIndex = 0;
				ShowSwitcherUI();
			}
			else {
				HideSwitcherUI();
			}
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam); 
	}
	return 0;
}

LRESULT CALLBACK SwitcherProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_TAB: 
			if (GetKeyState(VK_SHIFT) & 0x800) {
				SelectPreviousWindow();
			}
			else {
				SelectNextWindow();
			}
			InvalidateRect(hWnd, NULL, TRUE);
			break;
		case VK_ESCAPE:
			HideSwitcherUI();
			break;
		}
		break;
	case WM_KEYUP:
		if (wParam == VK_MENU) {
			if (g_hSwitcherWnd && IsWindowVisible(g_hSwitcherWnd) && !g_openWindows.empty() && g_selectedIndex != -1) {
				ActivateSelectedWindow();
			}
			HideSwitcherUI();
		}
		break;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		RECT clientRect;
		GetClientRect(hWnd, &clientRect);
		HBRUSH hBackgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
		FillRect(hdc, &clientRect, hBackgroundBrush);
		DeleteObject(hBackgroundBrush);

		HFONT hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
		HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, RGB(220, 220, 220));

		int yPos = 0;
		for (size_t i = 0; i < g_openWindows.size(); ++i) {
			RECT itemRect = { 10, yPos, clientRect.right - 10, yPos + 30 };
			if (i == g_selectedIndex) {
				HBRUSH hSelectionBrush = CreateSolidBrush(RGB(0, 120, 215));
				FillRect(hdc, &itemRect, hSelectionBrush);
				DeleteObject(hSelectionBrush);
			}
			DrawTextW(hdc, g_openWindows[i].title.c_str(), -1, &itemRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE); 
			yPos += 35;
		}

		SelectObject(hdc, hOldFont);
		DeleteObject(hFont);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_DESTROY:
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

std::vector<WindowInfo>* pWindowList = nullptr;

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
	if (!IsWindowVisible(hWnd) || GetWindow(hWnd, GW_OWNER) != NULL) {
		return TRUE;
	}
	int length = GetWindowTextLength(hWnd);
	if (length == 0) {
		return TRUE;
	}
	std::wstring title(length + 1, L'\0');
	GetWindowText(hWnd, &title[0], length + 1);
	title.resize(length);

	LONG_PTR exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
	if (exStyle & WS_EX_TOOLWINDOW) {
		return TRUE;
	}

	BOOL cloaked = FALSE;
	HRESULT hr = DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
	if (SUCCEEDED(hr) && cloaked) {
		return TRUE;
	}

	if (GetParent(hWnd) != NULL) {
		return TRUE;
	}

	pWindowList->push_back({ hWnd, title });
	return TRUE;
}

void EnumerateVisibleWindows() {
	g_openWindows.clear();
	pWindowList = &g_openWindows;
	EnumWindows(EnumWindowsProc, 0);
	pWindowList = nullptr;
}

void ActivateSelectedWindow() {
	if (g_selectedIndex >= 0 && g_selectedIndex < g_openWindows.size()) {
		HWND target_hWnd = g_openWindows[g_selectedIndex].hWnd;

		DWORD foregroundThreadID = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
		DWORD targetThreadID = GetWindowThreadProcessId(target_hWnd, NULL);

		if (foregroundThreadID != targetThreadID) {
			AttachThreadInput(foregroundThreadID, targetThreadID, TRUE);
		}

		SwitchToThisWindow(target_hWnd, TRUE);

		if (IsIconic(target_hWnd)) {
			ShowWindow(target_hWnd, SW_RESTORE);
		}

		if (foregroundThreadID != targetThreadID) {
			AttachThreadInput(foregroundThreadID, targetThreadID, FALSE);
		}
	}
}

void ShowSwitcherUI() {
	if (!g_hSwitcherWnd) {
		g_hSwitcherWnd = CreateWindowEx(
			WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
			SWITCHER_CLASS_NAME,
			L"AltTab Switcher",
			WS_POPUP,
			(GetSystemMetrics(SM_CXSCREEN) - 400) / 2, // Centered X
			(GetSystemMetrics(SM_CYSCREEN) - (g_openWindows.size() * 35 + 20)) / 2, // Centered Y, dynamically sized
			400, // Fixed width
			g_openWindows.size() * 35 + 20, // Height based on number of windows + padding
			NULL,
			NULL,
			hInst,
			NULL
		);
		if (!g_hSwitcherWnd) {
			return;
		}
	}
	else {
		SetWindowPos(g_hSwitcherWnd, HWND_TOPMOST,
			(GetSystemMetrics(SM_CXSCREEN) - 400) / 2,
			(GetSystemMetrics(SM_CYSCREEN) - (g_openWindows.size() * 35 + 20)) / 2,
			400, g_openWindows.size() * 35 + 20,
			SWP_NOACTIVATE | SWP_SHOWWINDOW);
	}
	ShowWindow(g_hSwitcherWnd, SW_SHOWNA); // Show without activating it
	SetFocus(g_hSwitcherWnd); // Try to set focus to capture keyboard input
	UpdateWindow(g_hSwitcherWnd);
}

void HideSwitcherUI() {
	if (g_hSwitcherWnd) {
		ShowWindow(g_hSwitcherWnd, SW_HIDE);
		g_selectedIndex = -1;
		g_openWindows.clear();
	}
}

void SelectNextWindow() {
	if (!g_openWindows.empty()) {
		g_selectedIndex = (g_selectedIndex + 1) % g_openWindows.size();
	}
}

void SelectPreviousWindow() {
	if (!g_openWindows.empty()) {
		g_selectedIndex = (g_selectedIndex - 1 + g_openWindows.size()) % g_openWindows.size();
	}
}