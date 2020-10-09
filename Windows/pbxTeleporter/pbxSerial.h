/* pbxSerial.h
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
#ifndef __serial_h__
#define __serial_h__

BOOL EnumerateSerialPorts(UINT* pNumber, TCHAR* pPortName, int strMaxLen);
void setDefaultSerialPort();
BOOL serialCanUsePort(int n);
HANDLE serialOpen(LPCWSTR device, DWORD baud);
int serialAvailable(HANDLE h);
void serialFlush(HANDLE h);
void serialClose(HANDLE h);
bool serialGetbyte(HANDLE h, uint8_t* b);
bool serialGetbytes(HANDLE h, uint8_t* buf, DWORD n);

void destroySerialListener();
unsigned __stdcall serialReadThread(LPVOID  arg);

#endif

