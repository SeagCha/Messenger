#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#include "server.h"

#pragma comment(lib, "Ws2_32.lib")

#define PORT L"26067"

int init_winsock(void);
int resolve_addresses(ADDRINFOT** addrinfo);
BOOL create_listen_socket(const ADDRINFOT* addrinfo, SOCKET* listen_socket);
BOOL begin_listening(SOCKET listen_socket);

void WorkingThread(User u);