/* pbxTeleporter.cpp
 *
 * Serial -> UDP Bridge for Pixelblaze
  *
 * Reads data from a Pixelblaze by emulating a single (8 channel, 2048 pixel) output expander
 * board and forwards it over the network via UDP datagram on request.  The wire protocol is
 * described in Ben Hencke's Pixelblaze Output Expander board repository at:
 * https://github.com/simap/pixelblaze_output_expander
 *
 * Part of the PixelTeleporter project
 * 2020 by JEM (ZRanger1)
 * Distributed under the MIT license
*/
#include "pbxTeleporter.h"

#define MAX_LOADSTRING 100

///////////////////////////////////////////////////////////////////////////////////////////
// Required Windows infrastructure
//////////////////////////////////////////////////////////////////////////////////////////

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
pbxTeleporterData Teleporter;

// Forward declarations 
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// Main entry point
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_PBXTELEPORTER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow)) {
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PBXTELEPORTER));
	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0)) {
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}

// Register main window class 
ATOM MyRegisterClass(HINSTANCE hInstance) {
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PBXTELEPORTER));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_PBXTELEPORTER);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

// Create and display main window
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
	hInst = hInstance;

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 640, 400, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd) { return FALSE; }

	SetTimer(hWnd, IDT_STATUS, 3000, (TIMERPROC) NULL);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return TRUE;
}

// Main window message handler
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	PAINTSTRUCT ps;
	int wmId;
	HDC hdc;

	switch (message) {
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId) {
		case IDM_START:
			break;
		case IDM_STOP:
			break;
		case IDM_PORTS:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_PORTS), hWnd, portsDialog);
			break;
		case IDM_SERIALDEVICE:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_SERIALPORT), hWnd, serialDialog);
			break;
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_CREATE:
		// create udp and serial handler threads
		 // Abort if we can't complete initialization;
		if (!Teleporter.start()) {
			return -1; 
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		UpdateStatusDisplay(hWnd, hdc);
		EndPaint(hWnd, &ps);
		break;
	case WM_TIMER:
		// Update status display on timer.
		switch (wParam) {
		case IDT_STATUS:
			InvalidateRect(hWnd, NULL, TRUE);
			UpdateWindow(hWnd);
		}
		break;
	case WM_CLOSE:
		Teleporter.settings.Save();
		Teleporter.shutdown();
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// methods for statistics and status display in main window
//////////////////////////////////////////////////////////////////////////////////////////
void UpdateStatusDisplay(HWND hwnd, HDC dc) {
	RECT rc;
	WCHAR string[256];
	UINT nPixels;

	nPixels = Teleporter.getPixelsReady();
	if (nPixels > 0) {
		StringCbPrintf(string, sizeof(string), L"Connected. Pixel count is: %u", nPixels);
	}
	else {
		StringCbPrintf(string, sizeof(string), L"Not Connected");
	}

//	if (!GetUpdateRect(hwnd, &rc, FALSE)) return;
	GetClientRect(hwnd, &rc);
	DrawText(dc, string, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}


///////////////////////////////////////////////////////////////////////////////////////////
// pbxSettings methods -- load/save/get/set persistent settings
//////////////////////////////////////////////////////////////////////////////////////////
LPCWSTR pbxSettings::GetSerialPortName() {
	return (LPCWSTR)serialDevice;
}

void pbxSettings::SetSerialPortName(LPCWSTR name) {
	wcsncpy(serialDevice, name, MAX_DEVICE_NAME_LEN);
}

LPCWSTR pbxSettings::GetBindIp() {
	return (LPCWSTR)bind_ip;
}

void pbxSettings::SetBindIp(LPCWSTR name) {
	wcsncpy(bind_ip, name, 16);
}

BOOL pbxSettings::Save() {
	BOOL result;
	WCHAR path[MAX_PATH];
	WCHAR tmp[MAX_DEVICE_NAME_LEN];

	// build ini path
	GetCurrentDirectoryW(MAX_PATH, path);
	wcscat_s(path, MAX_PATH, INI_NAME);

	// serial device name
	result = WritePrivateProfileStringW(L"settings", L"serialDevice", GetSerialPortName(), path);
	if (!result) return FALSE;

	// listen (command) port number
	_itow(Teleporter.settings.listenPort, tmp, 10);
	result = WritePrivateProfileStringW(L"settings", L"listenPort", tmp, path);
	if (!result) return FALSE;

	// send (data) port number
	_itow(Teleporter.settings.sendPort, tmp, 10);
	result = WritePrivateProfileStringW(L"settings", L"sendPort", tmp, path);

	return result;
}

void pbxSettings::Load() {
	WCHAR value[MAX_DEVICE_NAME_LEN];
	WCHAR dflt[MAX_DEVICE_NAME_LEN];
	WCHAR path[MAX_PATH];

	// build ini path
	GetCurrentDirectoryW(MAX_PATH, path);
	wcscat_s(path, MAX_PATH, INI_NAME);

	// serial device name
	GetPrivateProfileStringW(L"settings", L"serialDevice", GetSerialPortName(), value, MAX_DEVICE_NAME_LEN, path);
	if (wcslen(value)) {
		SetSerialPortName(value);
	}
	else {
		setDefaultSerialPort();
	}

	// bind ip address -- not supported in UI, must be manually entered by user.  Default is "" or 0.0.0.0
	// to bind to all available addresses.  User can specify a single IPv4 address for binding.
	GetPrivateProfileStringW(L"settings", L"bind_ip", NULL, value, MAX_DEVICE_NAME_LEN, path);
	SetBindIp(value);

	// listen (command) port number
	_itow(DEFAULT_LISTEN_PORT, dflt, 10);
	GetPrivateProfileStringW(L"settings", L"listenPort", dflt, value, MAX_DEVICE_NAME_LEN, path);
	listenPort = _wtoi(value);

	// send (data) port number
	_itow(DEFAULT_SEND_PORT, dflt, 10);
	GetPrivateProfileStringW(L"settings", L"sendPort", dflt, value, MAX_DEVICE_NAME_LEN, path);
	sendPort = _wtoi(value);
}


///////////////////////////////////////////////////////////////////////////////////////////
// pbxTeleporterData methods - operate the serial -> UDP bridge
//////////////////////////////////////////////////////////////////////////////////////////

// intialize serial and network communication and start processing data
BOOL pbxTeleporterData::start() {
	Teleporter.settings.Load();

	// initialize and enable the main loop
	runFlag = 1;
	dataReady = 0;
	resetPixelBuffer();

	serialHandle = serialOpen(settings.GetSerialPortName(), RCV_BITRATE);
	if (serialHandle == NULL) {
		return FALSE;
	}

	// start serial reader thread
	UINT threadID;
	serialThread = (HANDLE) _beginthreadex(NULL, 0, &serialReadThread, 0, 0, &threadID);
	if (serialThread == 0) {
		return FALSE;
	}

	// set up UDP server
	udp = createUdpServer((char*)"", settings.listenPort, settings.sendPort);
	if (udp == NULL) {
		return FALSE;
	}
	serialFlush(serialHandle);
	return TRUE;
}

void pbxTeleporterData::shutdown() {
	stop();
	destroyUdpServer(udp);
	destroySerialListener();
}

BOOL pbxTeleporterData::restart() {
	stop();
	Sleep(500);
	return start();
}



