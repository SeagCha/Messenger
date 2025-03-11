#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

#include "server_sockets.h"
#include "server.h"

#pragma comment(lib, "Ws2_32.lib")

void init_server(void)
{
    global_user_list = userlist_create();
    global_room_list = roomlist_create();

    roomlist_create_room(global_room_list, "default");
}

void boot_client_gracefully(const SOCKET client)
{
    printf("booted: %llu\n", client);
    shutdown(client, SD_BOTH);
}

void boot_user_gracefully(const SOCKET client, User* user)
{
    end_user_activities(user);
    userlist_remove(global_user_list, user->nickname);
    free(user);
    boot_client_gracefully(client);
}

struct ClientReceiveThreadArgs
{
    SOCKET client_socket;
    User* user;
};

void ClientReceiveThread(struct ClientReceiveThreadArgs* args)
{
    const SOCKET client_socket = args->client_socket;
    User* user = args->user;

    WSAPOLLFD fd = {0};
    fd.fd = client_socket;
    fd.events = POLLRDNORM | POLLWRNORM;
    fd.revents = 0;

    while (1)
    {
        fd.revents = 0;
        const int poll_result = WSAPoll(&fd, 1, CONNECTION_TIMEOUT);
        if (poll_result > 0)
        {
            if (fd.revents & POLLRDNORM)
            {
                char buffer[CHATPACKET_SIZE];
                const int result = recv(client_socket, buffer, CHATPACKET_SIZE, 0);
                if (result > 0)
                {
                    ChatPacket* deserialized = deserialize_packet(buffer);
                    process_packet(*deserialized, user);
                    free(deserialized);
                }
                else if (result == 0)
                {
                    boot_user_gracefully(client_socket, user);
                    free(args);
                    return;
                }
                else
                {
                    printf("recv error for socket %llu; booting\n", client_socket);
                    boot_user_gracefully(client_socket, user);
                    free(args);
                    return;
                }
            }
            else if (fd.revents & POLLERR || fd.revents & POLLHUP || fd.revents & POLLNVAL)
            {
                printf("disconnected: %llu\n", client_socket);
                boot_user_gracefully(client_socket, user);
                free(args);
                return;
            }
        }
        else if(poll_result == SOCKET_ERROR)
        {
            printf("poll error for socket %llu; booting\n", client_socket);
            boot_user_gracefully(client_socket, user);
            free(args);
            return;
        }
    }
}

int main(void)
{
    init_server();

    init_winsock();

    ADDRINFOT* addrinfo = NULL;
    int retval = resolve_addresses(&addrinfo);
    if (addrinfo == NULL)
    {
        printf("Error in addrinfo: %d\n", retval);
        WSACleanup();
        return 1;
    }

    SOCKET listen_socket;
    retval = create_listen_socket(addrinfo, &listen_socket);
    FreeAddrInfo(addrinfo);
    if (retval == FALSE)
    {
        printf("Error in create_listen_socket: %d\n", WSAGetLastError());
        if (listen_socket != INVALID_SOCKET)
            closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    retval = begin_listening(listen_socket);
    if (retval == FALSE)
    {
        printf("Error in begin_listening: %d\n", WSAGetLastError());
        FreeAddrInfo(addrinfo);
        if (listen_socket != INVALID_SOCKET)
            closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    WSAPOLLFD fd = {0};
    fd.fd = listen_socket;
    fd.events = POLLRDNORM;
    fd.revents = 0;

    while(TRUE)
    {
        const int poll_result = WSAPoll(&fd, 1, CONNECTION_TIMEOUT);
        if (poll_result > 0)
        {
            if (fd.revents & POLLRDNORM)
            {
                SOCKADDR_STORAGE client_sockaddr = {0};
                int addrlen = sizeof(SOCKADDR_STORAGE);
                const SOCKET accepted_client_socket = accept(listen_socket, (SOCKADDR*)&client_sockaddr, &addrlen);
                if (accepted_client_socket != INVALID_SOCKET)
                {
                    // info
                    wchar_t addrstr[INET6_ADDRSTRLEN];
                    int addrstrlen = INET6_ADDRSTRLEN;
                    WSAAddressToString((SOCKADDR*)&client_sockaddr, sizeof(SOCKADDR_STORAGE), NULL, (LPWSTR)addrstr, (LPDWORD)&addrstrlen);
                    printf("accepted: %ls on socket %llu\n", addrstr, accepted_client_socket);

                    // start thread
                    struct ClientReceiveThreadArgs* thread_args = malloc(sizeof(struct ClientReceiveThreadArgs));
                    thread_args->client_socket = accepted_client_socket;

                    // create user
                    User* user = (User*)malloc(sizeof(User));
                    ZeroMemory(user, sizeof(User));
                    user->socket = accepted_client_socket;
                    userlist_add(global_user_list, user);

                    thread_args->user = user;
                    HANDLE client_thread_handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ClientReceiveThread, thread_args, 0, NULL);

                    if(client_thread_handle == NULL)
                    {
                        boot_client_gracefully(accepted_client_socket);
                        free(thread_args);
                        continue;
                    }

                    CloseHandle(client_thread_handle);
                }
                else
                {
                    printf("accept failed\n");
                }
            }
        }
        else if (poll_result == SOCKET_ERROR)
        {
            printf("poll failed\n");
        }
    }

    WSACleanup();
    return 0;
}