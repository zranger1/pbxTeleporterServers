/* pbxSerial.cpp
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

///////////////////////////////////////////////////////////////////////////////////////////
// Utilities for working with serial devices
//////////////////////////////////////////////////////////////////////////////////////////

// Enumerate serial ports by calling QueryDosDevice()
// Code adapted from method by Gaiger Chen 
// http://gaiger-programming.blogspot.com/2015/07/methods-collection-of-enumerating-com.html
BOOL EnumerateSerialPorts(UINT* pNumber, TCHAR* portList, int strMaxLen) {
    int devCount = 0;

    // allocate hideously large buffer because QueryDosDevice returns very
    // long descriptors for every device in the system.  There are a *lot* of devices.
    UINT nChars = 65535;
    WCHAR* pDevices = new WCHAR[nChars];
    DWORD dwChars = QueryDosDeviceW(NULL, &pDevices[0], nChars);

    // if we fail here, it's probably because the buffer isn't big enough.
    // TODO -- expand the buffer and try again?
    if (dwChars) {

        // loop through the device list looking for COM ports.  
        int i = 0;
        while (pDevices[i]) {
            WCHAR* pszCurrentDevice = &pDevices[i];
            int nLen = (int)wcslen(pszCurrentDevice);
            if (nLen > 3) {
                if ((0 == _wcsnicmp(pszCurrentDevice, L"COM", 3)) && isdigit(pszCurrentDevice[3])) {
                    wcsncpy(portList + static_cast<unsigned __int64>(devCount) * strMaxLen, pszCurrentDevice, strMaxLen);
                    devCount++;
                }
            }
            i += (nLen + 1);
        }
    }
    *pNumber = devCount;
    delete[] pDevices;
    return (devCount > 0);
}

// given a serial port number (n, as in COMn), returns TRUE if that port exists
// and can be opened at the speed we require, FALSE otherwise.
BOOL serialCanUsePort(int n) {
    WCHAR name[32];
    HANDLE h;

    wsprintf(name,L"\\\\.\\COM%i",n);
    h = serialOpen(name, RCV_BITRATE);
    if (h == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    CloseHandle(h);
    return TRUE;
}

// if the user has not (yet) specified a serial device, pick the first available
// if no serial port is currently available, do nothing 
void setDefaultSerialPort() {
    UINT nPorts;
    BOOL result;
    WCHAR* portList;

// if a port is already set, we're good!
    if (wcslen(Teleporter.settings.GetSerialPortName())) return;

// otherwise, grab the list of serial devices
    portList = new WCHAR[MAX_SERIAL_PORTS * MAX_DEVICE_NAME_LEN];
    result = EnumerateSerialPorts(&nPorts, portList, MAX_DEVICE_NAME_LEN);
    if (result) {

// take the first valid serial port we find.  It's a good bet newer machines.
        if (nPorts) {
            Teleporter.settings.SetSerialPortName(portList);
        }
    }
    delete[] portList;
}

// Open and initialize serial port
HANDLE serialOpen(LPCWSTR device, DWORD speed) {
    HANDLE h;
    DCB params;
    COMMTIMEOUTS timeouts;
    BOOL bResult;

// open the serial device
    h = CreateFile(device,                 
            GENERIC_READ | GENERIC_WRITE,  
            0,                            // No sharing
            NULL,                         // No security
            OPEN_EXISTING,
            0,                            // Non overlapped I/O
            NULL);

    if (h == INVALID_HANDLE_VALUE) {
        return h;
    }

// set up device control block and get current device settings
    params = { 0 };                        
    params.DCBlength = sizeof(params);
    bResult = GetCommState(h, &params); 

    if (bResult == FALSE) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

// configure device settings
    params.BaudRate = speed;
    params.ByteSize = 8;
    params.StopBits = ONESTOPBIT;
    params.Parity   = NOPARITY;

    bResult = SetCommState(h, &params);

    if (bResult == FALSE) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

// set device timeouts to zero -- the device will block until the specified
// buffer is filled, which is fine because we won't be reading characters until
// we know they're present.
    timeouts = { 0 };
    bResult = SetCommTimeouts(h, &timeouts);
    if (bResult == FALSE) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

    return h;
}

bool serialGetbyte(HANDLE h, uint8_t* b) {
    DWORD dwBytesRead;
    return ReadFile(h, b, 1, &dwBytesRead, NULL);
}

bool serialGetbytes(HANDLE h, uint8_t* buf, DWORD n ) {
    DWORD dwBytesRead;
    return ReadFile(h, buf, n, &dwBytesRead, NULL);
}

// return bytes of data available on serial port. Ignores
// errors returned from ioctl
int serialAvailable(HANDLE h) {
    DWORD errors;
    COMSTAT stat;

    ClearCommError(h, &errors, &stat);

    return stat.cbInQue;
}

// flush tx/rx serial buffers
void serialFlush(HANDLE h) {
    PurgeComm(h, PURGE_RXCLEAR);
}

// close serial port
void  serialClose(HANDLE h) {
    CloseHandle(h);
}

//
// Reads the specified number of bytes into a buffer
void readBytes(uint8_t* buf, uint16_t size) {
    serialGetbytes(Teleporter.serialHandle, buf,size);
}

// read a single byte from the serial device
uint8_t readOneByte() {
    uint8_t b;
    serialGetbyte(Teleporter.serialHandle, &b);
    return b;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Tools for parsing serial output from Pixelblaze
//////////////////////////////////////////////////////////////////////////////////////////

// readMagicWord
// returns true if we've found the magic word "UPXL"
// false otherwise. Clunky, but fast.
bool readMagicWord() {
    if (readOneByte() != 'U') return false;
    if (readOneByte() != 'P') return false;
    if (readOneByte() != 'X') return false;
    if (readOneByte() != 'L') return false;

    return true;
}

// crcCheck()
// read and discard 32-bit CRC from data buffer 
void crcCheck() {
    uint32_t crc;
    readBytes((uint8_t*)&crc, sizeof(crc));
}

// read pixel data in WS2812 format
// NOTE: Only handles 3 byte RGB data for now.  Discards frame
// if it's any other size.
void doSetChannelWS2812() {
    PBWS2812Channel ch;
    uint16_t data_length;

    readBytes((uint8_t*)&ch, sizeof(ch));

    // read pixel data if available
    if (ch.pixels && (ch.numElements == 3) && (ch.pixels <= MAX_PIXELS)) {
        data_length = ch.pixels * ch.numElements;
        readBytes(Teleporter.pixel_ptr, data_length);
        Teleporter.pixel_ptr += data_length;
    }

    crcCheck();
}

// read pixel data in APA 102 format
void doSetChannelAPA102() {
    PBAPA102DataChannel ch;

    readBytes((uint8_t*)&ch, sizeof(ch));

    // APA 102 data is always four bytes. The first byte
    // contains a 3 bit flag and 5 bits of "extra" brightness data.
    // We're gonna discard the "extra" APA bits and put 3-byte RGB
    // data into the output buffer.
    if (ch.frequency && (ch.pixels <= MAX_PIXELS)) {
        for (int i = 0; i < ch.pixels; i++) {
            readOneByte();
            readBytes(Teleporter.pixel_ptr, 3);
            Teleporter.pixel_ptr += 3;
        }
    }
    crcCheck();
}

// draw all pixels on all channels using current data
// flags the frame as available to the network transport.
void doDrawAll() {
    Teleporter.calcDataSize();
    Teleporter.resetPixelBuffer();
}

// read APA 102 clock data.  
// For now, we ignore this. Eventually, we may have to at least keep the
// desired frequency for virtual wiring
void doSetChannelAPA102Clock() {
    PBAPA102ClockChannel ch;

    readBytes((uint8_t*)&ch, sizeof(ch));
    crcCheck();
}

// clean up all serial device related threads and handles
void destroySerialListener() {

    // close serial port if open. Do this before killing thread to unblock
    // any pending serial reads
    if (Teleporter.serialHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(Teleporter.serialHandle);
    }

    // if thread exists, wait for it to terminate, then clean up
    if (Teleporter.serialThread != 0) {
        WaitForSingleObject(Teleporter.serialThread, INFINITE);
        CloseHandle(Teleporter.serialThread);
    }
}

// get pixel data from the serial device, forward packets to clients
// via UDP
unsigned __stdcall serialReadThread(LPVOID  arg) {
    PBFrameHeader hdr;

    // loop forever
    while (Teleporter.runFlag) {
        // read characters 'till we get the magic sequence
        if (readMagicWord()) {
            readBytes((uint8_t*)&hdr, sizeof(hdr));

            switch (hdr.command) {
            case SET_CHANNEL_WS2812:
                doSetChannelWS2812();
                break;
            case DRAW_ALL:
                doDrawAll();
                break;
            case SET_CHANNEL_APA102_DATA:
                doSetChannelAPA102();
                break;
            case SET_CHANNEL_APA102_CLOCK:
                doSetChannelAPA102Clock();
                break;
            default:
                break;
            }
        }
    }

    return 0;
}




