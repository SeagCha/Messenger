#include "server_sockets.h"

#include <stdio.h>

int init_winsock(void)
{
    WSADATA wsaData;
    const int retval = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return retval;
}

int resolve_addresses(ADDRINFOT** addrinfo)
{
    ADDRINFOT hints;
    ZeroMemory(&hints, sizeof(ADDRINFOT));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    const int retval = GetAddrInfo(NULL, PORT, &hints, addrinfo);
    return retval;
}

BOOL create_listen_socket(const ADDRINFOT* addrinfo, SOCKET* listen_socket)
{
    *listen_socket = INVALID_SOCKET;

    *listen_socket = WSASocket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol, NULL, 0, 0);

    if (*listen_socket == INVALID_SOCKET)
        return FALSE;

    int retval = bind(*listen_socket, addrinfo->ai_addr, (int)addrinfo->ai_addrlen);
    if (retval == SOCKET_ERROR)
        return FALSE;

    char hoststr[NI_MAXHOST], servstr[NI_MAXSERV];
    retval = getnameinfo(
        addrinfo->ai_addr,
        (socklen_t)addrinfo->ai_addrlen,
        hoststr,
        NI_MAXHOST,
        servstr,
        NI_MAXSERV,
        NI_NUMERICHOST | NI_NUMERICSERV
    );
    if (retval != 0)
        printf("getnameinfo failed: %d\n", retval);
    else
        printf("Socket %llu bound to address %s and port %s\n", *listen_socket, hoststr, servstr);

    return TRUE;
}

BOOL begin_listening(const SOCKET listen_socket)
{
    const int retval = listen(listen_socket, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        return FALSE;
    return TRUE;
}