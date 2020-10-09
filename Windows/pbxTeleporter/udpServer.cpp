/* udpServer.cpp
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
#pragma comment(lib, "Ws2_32.lib")
#include "pbxTeleporter.h"

#define UDP_INBUFSIZE 256 // size of buffer for incoming UPD data.

udpServer* createUdpServer(char* bind_addr, int listen_port, int send_port) {
	udpServer* udp;
	int options;
	int result;
	WSADATA wsa;

	// initialise winsock v2.2
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		return NULL;
	}

	udp = new udpServer;
	if (udp == NULL) {
		return NULL;
	}

	// open socket	
	udp->fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (udp->fd < 0) {
		destroyUdpServer(udp);
		return NULL;
	}

	// Allow address reuse to get rid of the "Address already in use" error and/or
	// delay when we need to restart the server
	options = 1;
	result = setsockopt(udp->fd, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&options, sizeof(int));

	// configure listening address
	udp->listen_port = listen_port;
	udp->send_port = send_port;
	udp->server.sin_family = AF_INET;
	udp->server.sin_port = htons(listen_port);

	if (strlen(bind_addr) == 0) {
		udp->server.sin_addr.s_addr = INADDR_ANY;
	}
	else {
		udp->server.sin_addr.s_addr = inet_addr(bind_addr);
	}

	// bind socket to address  
	if (bind(udp->fd, (struct sockaddr*)&udp->server, sizeof(struct sockaddr_in)) < 0) {
		destroyUdpServer(udp);
		return NULL;
	}

	// create listener thread
	UINT threadID;
	udp->pt = (HANDLE) _beginthreadex(NULL, 0, &udpThread, 0, 0, &threadID); 
	if (udp->pt == 0) {
		destroyUdpServer(udp);
	}

	return udp;
}

// send/receive fns
int udpServerListen(udpServer* udp, char* rcvbuf, size_t bufsize) {
	udp->clientlen = sizeof(struct sockaddr);

	return recvfrom(udp->fd, rcvbuf, (int) bufsize, 0,
		(struct sockaddr*)&udp->client, (socklen_t*)&udp->clientlen);
}

int udpServerSend(udpServer* udp, char* sendbuf, size_t bufsize) {
	udp->client.sin_port = htons(udp->send_port);
	return sendto(udp->fd,  sendbuf, (int) bufsize, 0,
		(struct sockaddr*)&udp->client, udp->clientlen);
}

// clean up all network related handles and memory
void destroyUdpServer(udpServer* udp) {
	if (udp == NULL) return;

	// close socket if open and stop winsock2.  This ought
	// to break recvFrom out of its blocking wait.
	if (udp->fd >= 0) closesocket(udp->fd);

    // if thread exists, wait for it to terminate, then clean up
	if (udp->pt != 0) {
		WaitForSingleObject(udp->pt, INFINITE);
		CloseHandle(udp->pt);
	}

	WSACleanup();

	// free server object 
	if (udp != NULL) {
		delete udp;
	}
}

// UDP Server thread function. Once data becomes available, does a blocking
// listen for requests, and forwards the pixel data when it gets one. Net
// data rate is decoupled from Pixelblaze frame rate, and multiple clients
// are supported, although you're gonna need a sturdy router for that...
// TODO - implement protocol for virtual wiring
unsigned __stdcall udpThread(LPVOID  arg) {
	uint8_t incoming_buffer[UDP_INBUFSIZE];
	int res;

	while (Teleporter.runFlag) {
		if (Teleporter.isDataReady()) {
			res = udpServerListen(Teleporter.udp,(char *) incoming_buffer, UDP_INBUFSIZE);

			if (res > 0) {
				udpServerSend(Teleporter.udp,(char *) Teleporter.pixel_buffer, Teleporter.getDataReady());
			}
		}
	}

	return 0;
}
