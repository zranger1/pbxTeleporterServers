/* pbxTeleporter.h
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
#ifndef __pbxteleporter_h__
#define __pbxteleporter_h__
#define WIN32_LEAN_AND_MEAN             
#include "resource.h"
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <process.h>
#include <strsafe.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2def.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "pbxSerial.h"
#include "udpServer.h"

#define RCV_BITRATE    2000000L           // bits/sec coming from pixelblaze
#define DEFAULT_MAX_PIXELS 2048           // 2k pixels plus
#define DEFAULT_LISTEN_PORT   8081        // default UDP ports
#define DEFAULT_SEND_PORT 8082
#define MAX_SERIAL_PORTS      128
#define MAX_DEVICE_NAME_LEN       64
#define INI_NAME L"\\pbxTeleporter.ini"
#define DISCONNECT_TIMEOUT 5000

#define IDT_STATUS 5005

 ///////////////////////////////////////////////////////////////////////////////////////////
 // structures imported from pixelblaze expander source
 // https://github.com/simap/pixelblaze_output_expander
 //////////////////////////////////////////////////////////////////////////////////////////
#pragma pack(push,1)
enum RecordType {
    SET_CHANNEL_WS2812 = 1, DRAW_ALL, SET_CHANNEL_APA102_DATA, SET_CHANNEL_APA102_CLOCK
};

typedef struct {
    //    int8_t magic[4];
    uint8_t channel;
    uint8_t command;
} PBFrameHeader;

typedef struct {
    uint8_t numElements; //0 to disable channel, usually 3 (RGB) or 4 (RGBW)
    union {
        struct {
            uint8_t redi : 2, greeni : 2, bluei : 2, whitei : 2; //color orders, data on the line assumed to be RGB or RGBW
        };
        uint8_t colorOrders;
    };
    uint16_t pixels;
} PBWS2812Channel;

typedef struct {
    uint32_t frequency;
    union {
        struct {
            uint8_t redi : 2, greeni : 2, bluei : 2; //color orders, data on the line assumed to be RGB
        };
        uint8_t colorOrders;
    };
    uint16_t pixels;
}  PBAPA102DataChannel;

typedef struct {
    uint32_t frequency;
} PBAPA102ClockChannel;
#pragma pack(pop)

///////////////////////////////////////////////////////////////////////////////////////////
// Persistent settings
//////////////////////////////////////////////////////////////////////////////////////////
class pbxSettings {
public:
    WCHAR serialDevice[MAX_DEVICE_NAME_LEN] = { 0 };     // saved/default settings
    WCHAR bind_ip[16] = { 0 };                           // local ip address for socket bind
    int listenPort = DEFAULT_LISTEN_PORT;
    int sendPort = DEFAULT_SEND_PORT;
    int maxPixels = DEFAULT_MAX_PIXELS;

    BOOL Save(); 
    void Load();
    LPCWSTR GetSerialPortName();
    void SetSerialPortName(LPCWSTR name);
    LPCWSTR GetBindIp();
    void SetBindIp(LPCWSTR name);
};


///////////////////////////////////////////////////////////////////////////////////////////
// PixelTeleporter app and thread data in one convenient bundle
//////////////////////////////////////////////////////////////////////////////////////////
class pbxTeleporterData {
public:
    ~pbxTeleporterData();

    BOOL runFlag = TRUE;                        // app is running -- set FALSE to shut down
    UINT dataReady = 0;                         // bytes of valid pixel data in the frame buffer
    UINT buffer_size = 0;                       // size of memory allocated for pixel buffer
    uint8_t* pixel_buffer = NULL;               // frame buffer holding pixels read from controller
    uint8_t* pixel_ptr = NULL;                  // pointer to current location in pixel_buffer
    HANDLE serialHandle = INVALID_HANDLE_VALUE; // handle to active serial device
    HANDLE serialThread = 0;                    // handle to serial listener thread
    DWORD  frameTimer = 0;                      // watchdog timer for connection
    udpServer* udp = NULL;                      // pointer to udp listener object

    pbxSettings settings;                       // stored/default settings

    void calcDataSize() { dataReady = (UINT)(pixel_ptr - pixel_buffer); }
    BOOL isDataReady() { return dataReady > 0; }
    UINT getDataReady() { return dataReady; }
    int getPixelsReady() { return (int)dataReady / 3; }
    void resetPixelBuffer() { pixel_ptr = pixel_buffer; }
    void clearAllData() { resetPixelBuffer();  dataReady = 0; }
    void updateFrameTimer() { frameTimer = GetTickCount(); }
    DWORD getTimeSinceLastFrame() { return GetTickCount() - frameTimer; }
    BOOL createFrameBuffer(int nPixels);

    void stop() { runFlag = FALSE; }
    BOOL start();
    void shutdown();
    BOOL restart();
};

extern pbxTeleporterData Teleporter;

// dialog window handlers;
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK portsDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK serialDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// utility functions
void UpdateStatusDisplay(HWND hwnd, HDC dc);

#endif /* __pbxteleporter_h__ */

