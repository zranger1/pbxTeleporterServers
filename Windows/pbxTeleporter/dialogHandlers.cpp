/* dialogHandlers.cpp
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

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// set control values for Ports dialog
void initPortsDialog(HWND hDlg) {
	WCHAR str[10];
	SendDlgItemMessage(hDlg, IDC_LISTEN_PORT, EM_LIMITTEXT, (WPARAM)5, 0);
	SendDlgItemMessage(hDlg, IDC_SEND_PORT, EM_LIMITTEXT, (WPARAM)5, 0);

	_itow(Teleporter.settings.sendPort ,str, 10);
	SendDlgItemMessage(hDlg, IDC_SEND_PORT, WM_SETTEXT, 0,(LPARAM) str);

	_itow(Teleporter.settings.listenPort, str, 10);
	SendDlgItemMessage(hDlg, IDC_LISTEN_PORT, WM_SETTEXT, 0, (LPARAM) str);
}

// read and validate control values for Ports dialog
// returns TRUE if values were changed, FALSE otherwise
BOOL readPortsDialog(HWND hDlg) {
	WCHAR str[10] = { 0 };
	int portNo;
	BOOL bChanged = FALSE;

// send port
	SendDlgItemMessage(hDlg, IDC_SEND_PORT, WM_GETTEXT, (WPARAM)10, (LPARAM)str);
	if (wcslen(str) == 0) {
		portNo = DEFAULT_SEND_PORT;
	}
	else {
		portNo = _wtoi(str);
	}

	bChanged = bChanged || (portNo == Teleporter.settings.sendPort);
	Teleporter.settings.sendPort = portNo;

// listen port
	SendDlgItemMessage(hDlg, IDC_LISTEN_PORT, WM_GETTEXT, (WPARAM)10, (LPARAM)str);
	if (wcslen(str) == 0) {
		portNo = DEFAULT_LISTEN_PORT;
	}
	else {
		portNo = _wtoi(str);
	}
	bChanged = bChanged || (portNo == Teleporter.settings.listenPort);
	Teleporter.settings.listenPort = portNo;

	return bChanged;
}

// Message handler for ports dialog
INT_PTR CALLBACK portsDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);

	switch (message) {
	case WM_INITDIALOG:
		initPortsDialog(hDlg);
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			if (readPortsDialog(hDlg)) {
				Teleporter.settings.Save();
				Teleporter.restart();
				EndDialog(hDlg, LOWORD(wParam));
			}
			return (INT_PTR)TRUE;
		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			break;
		default:
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Set controls for the Serial Device dialog
void initSerialComboBox(HWND hDlg) {
	UINT nPorts;
	BOOL result;
	WCHAR *portList; 
	portList = new WCHAR[MAX_SERIAL_PORTS*MAX_DEVICE_NAME_LEN];

	result = EnumerateSerialPorts(&nPorts, portList, MAX_DEVICE_NAME_LEN);
	if (result) {
		// add port names to combobox
		WCHAR* tmp = &(portList[0]);
		for (UINT i = 0; i < nPorts; i++) {
			WCHAR* tmp = portList + static_cast<unsigned __int64>(i) * MAX_DEVICE_NAME_LEN;
			SendDlgItemMessage(hDlg, IDC_SERIALLIST, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)tmp);
		}

		// TODO - use the saved or default item
		SendDlgItemMessage(hDlg, IDC_SERIALLIST, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
	}
	delete[] portList;
}

// read and validate controls for the Serial Device dialog
// returns TRUE if values changed, FALSE otherwise
BOOL readSerialComboBox(HWND hDlg) {
	int index;
	BOOL bChanged = FALSE;
	WCHAR port[MAX_DEVICE_NAME_LEN] = { 0 };

	index = (int) SendDlgItemMessage(hDlg, IDC_SERIALLIST, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
	if (index == CB_ERR) {
		// no serial ports, or nothing selected.  We set the port to empty string and let
		// the default port selector deal with it.
		bChanged = (wcscmp(L"",Teleporter.settings.GetSerialPortName()) != 0);
		Teleporter.settings.SetSerialPortName(L"");
	}
	else {
		SendDlgItemMessage(hDlg, IDC_SERIALLIST, WM_GETTEXT, (WPARAM)MAX_DEVICE_NAME_LEN, (LPARAM)port);
		bChanged = (wcscmp(port, Teleporter.settings.GetSerialPortName()) != 0);
		Teleporter.settings.SetSerialPortName(port);
	}

	return bChanged;
}

// Message handler for serial device dialog
INT_PTR CALLBACK serialDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);

	switch (message) {
	case WM_INITDIALOG:
		initSerialComboBox(hDlg);
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			if (readSerialComboBox(hDlg)) {
				Teleporter.settings.Save();
				Teleporter.restart();
			}
			EndDialog(hDlg, LOWORD(wParam));
		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			break;
		default:
			return (INT_PTR)TRUE;
		}
	}
	return (INT_PTR)FALSE;
}