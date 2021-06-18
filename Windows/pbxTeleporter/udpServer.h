/* udpServer.h
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
#ifndef __UDPSERVER_H
#define __UDPSERVER_H

typedef struct _udpServer {
	int listen_port = 8081;
	int send_port = 8082;
	SOCKET fd = -1;
	struct sockaddr_in server = { 0 };
	struct sockaddr_in client = { 0 };
	int clientlen = sizeof(struct sockaddr_in);
    HANDLE pt = INVALID_HANDLE_VALUE; 
	BOOL clientRequestFlag = FALSE;
} udpServer;

// void _debugPrintAddress(struct sockaddr_in* addr);
udpServer* createUdpServer(char* bind_addr, int listen_port, int send_port);
int udpServerListen(udpServer* udp, char* rcvbuf, size_t bufsize);
int udpServerSend(udpServer* udp, char* sendbuf, size_t bufsize);
void destroyUdpServer(udpServer* udp);
unsigned __stdcall udpThread(LPVOID  arg);

#endif
