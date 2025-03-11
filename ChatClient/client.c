#include "client.h"

#include <stdio.h>
#include <stdlib.h>

ChatPacket* create_packet_from_int(const int type, const int n)
{
    ChatPacket* packet = (ChatPacket*)malloc(sizeof(ChatPacket));
    packet->type = type;
    ZeroMemory(packet->data, CHATPACKET_MAX_DATA);
    sprintf(packet->data, "%d", n);
    return packet;
}

ChatPacket* create_packet_from_string(const int type, const char* str)
{
    ChatPacket* packet = (ChatPacket*)malloc(sizeof(ChatPacket));
    packet->type = type;
    ZeroMemory(packet->data, CHATPACKET_MAX_DATA);
    strncpy(packet->data, str, min(strlen(str), CHATPACKET_MAX_DATA));

    return packet;
}

ChatPacket* create_packet_from_full_string(const int type, const char* str)
{
    ChatPacket* packet = (ChatPacket*)malloc(sizeof(ChatPacket));
    packet->type = type;
    ZeroMemory(packet->data, CHATPACKET_MAX_DATA);
    memcpy(packet->data, str, CHATPACKET_MAX_DATA);

    return packet;
}

char* serialize_packet(const ChatPacket packet)
{
    char* result = (char*)malloc(CHATPACKET_SIZE * sizeof(char));
    memcpy(result, &packet.type, sizeof(int));
    memcpy(result + sizeof(int), packet.data, CHATPACKET_MAX_DATA * sizeof(char));
    return result;
}

ChatPacket* deserialize_packet(const char* source)
{
    ChatPacket* result = (ChatPacket*)malloc(sizeof(ChatPacket));
    memcpy(&result->type, source, sizeof(int));
    memcpy(result->data, source + sizeof(int), CHATPACKET_MAX_DATA * sizeof(char));
    return result;
}

int init_winsock(void)
{
    WSADATA wsaData;
    const int retval = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return retval;
}

void set_hints(ADDRINFO* hints)
{
    ZeroMemory(hints, sizeof(ADDRINFO));
    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_protocol = IPPROTO_TCP;
}

int get_addrinfo(const ADDRINFO* hints, const char* server_ip, ADDRINFO** result) //do
{
    const int retval = getaddrinfo(server_ip, PORT, hints, result); // "упаковывает" необходимый нам адресс в правильный формат
    return retval;
}

SOCKET create_socket(const ADDRINFO* result)
{
    SOCKET connect_socket = INVALID_SOCKET;

    connect_socket = socket(result->ai_family, // создает совет подходящим под мои требования
        result->ai_socktype,
        result->ai_protocol);

    return connect_socket;
}

int connect_to_server(const SOCKET socket, const ADDRINFO* addrinfo) //do
{
    const int retval = connect(socket, addrinfo->ai_addr, (int)addrinfo->ai_addrlen); // соединение сокета с сокетом найденным по адресу (забнженным на сервере)
    return retval;
}

int authorize_user(const SOCKET socket, const char* nickname)
{
    ChatPacket* packet = create_packet_from_string(Authorize, nickname);
    char data[CHATPACKET_SIZE];

    int retval = send_packet(socket, packet);
    if (retval <= 0) {
        printf("ERROR: send (authorize_user)\n");
        return -1;
    }

    retval = recv(socket, data, CHATPACKET_SIZE, 0);

    if (retval <= 0) {
        return -1;
    }

    free(packet);

    packet = deserialize_packet(data);

    if (packet->type == AuthorizeResult) {
        if (packet->data[0] == '1') return 0; else return 1;
    }
    else {
        return -1;
    }
}

void start_dm_session(SOCKET socket, char* recipient_name)
{
    system("cls");
    printf("Started chat with %s.\n", recipient_name);
    current_app_state = DMs;
    ZeroMemory(dm_recipient_name, ENTITY_NAME_MAX_LENGTH);
    strncpy(dm_recipient_name, recipient_name, ENTITY_NAME_MAX_LENGTH);

    printf("\x1b[90m");

    HANDLE threads[2];
    threads[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, (LPVOID)&socket, 0, 0);
    threads[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SendThread, (LPVOID)&socket, 0, 0);

    WaitForMultipleObjects(2, threads, TRUE, INFINITE);

    CloseHandle(threads[0]);
    CloseHandle(threads[1]);

    is_exiting_threads = FALSE;

    printf("\x1b[0m");
}

ChatPacket* process_user_input(SOCKET socket, const char* input) //do
{
    if (strncmp(input, "@dm", 3) == 0) {
        char recipient_nickname[50];
        char actual_message[CHATPACKET_MAX_DATA - ENTITY_NAME_MAX_LENGTH];
        sscanf(input, "@dm %s %973[^\n]", recipient_nickname, actual_message);
        char* formatted_data = format_dm_data(actual_message, recipient_nickname);
        ChatPacket* packet = create_packet_from_full_string(MessagePrivate, formatted_data);
        free(formatted_data);
        return packet;
    }

    if (current_app_state == Rooms && strcmp(input, "@room") == 0) {
        ChatPacket* packet = create_packet_from_string(MessageRoom, input);
        return packet;
    }

    if (current_app_state == Rooms && strcmp(input, "@allusers") == 0) {
        ChatPacket* packet = create_packet_from_string(MessageRoom, input);
        return packet;
    }

    if (strcmp(input, "@leave") == 0) {
        if (current_app_state == Rooms) {
            ChatPacket* packet = create_packet_from_int(LeaveRoom, 0);
            current_app_state = Idle;
            is_exiting_threads = TRUE;
            return packet;
        }

        is_exiting_threads = TRUE;
        current_app_state = Idle;
        return NULL;
    }

    if (current_app_state == Rooms) {
        ChatPacket* packet = create_packet_from_string(MessageRoom, input);
        return packet;
    }

    if (current_app_state == DMs) {
        char* formatted_data = format_dm_data(input, dm_recipient_name);
        ChatPacket* packet = create_packet_from_full_string(MessagePrivate, formatted_data);
        free(formatted_data);
        return packet;
    }

    return NULL;
}

char* format_dm_data(const char* input, const char* recepient)
{
    char* buf = malloc(CHATPACKET_MAX_DATA * sizeof(char));
    ZeroMemory(buf, CHATPACKET_MAX_DATA);

    char trimmed_input[CHATPACKET_MAX_DATA - ENTITY_NAME_MAX_LENGTH];
    ZeroMemory(trimmed_input, CHATPACKET_MAX_DATA - ENTITY_NAME_MAX_LENGTH);

    strncpy(trimmed_input, input, CHATPACKET_MAX_DATA - ENTITY_NAME_MAX_LENGTH - 1);
    strncpy(buf, recepient, ENTITY_NAME_MAX_LENGTH);
    strncpy(&buf[ENTITY_NAME_MAX_LENGTH], trimmed_input, CHATPACKET_MAX_DATA - ENTITY_NAME_MAX_LENGTH);

    return buf;
}

void ReceiveThread(const SOCKET* socket)
{
    WSAPOLLFD fd = { 0 };
    fd.fd = *socket;
    fd.events = POLLRDNORM | POLLWRNORM; // +3 ивента ошибки
    fd.revents = 0;

    while (1)
    {
        if (is_exiting_threads == TRUE) return;

        const int poll_result = WSAPoll(&fd, 1, CONNECTION_TIMEOUT);

        if (poll_result > 0)
        {
            if (fd.revents & POLLRDNORM) // проверка на то, что не ошибка
            {
                char* buf = (char*)malloc(CHATPACKET_SIZE * sizeof(char));
                int retval = recv(*socket, buf, CHATPACKET_SIZE, 0);
                if (retval != SOCKET_ERROR && retval > 0)
                {
                    ChatPacket* packet = deserialize_packet(buf);
                    on_receive_packet(packet);
                    free(packet);
                }
                else {
                    printf("ERROR: recv (receivethread)\n");
                }
                free(buf);
            }
            else if (fd.revents & POLLERR || fd.revents & POLLHUP || fd.revents & POLLNVAL)
            {
                printf("disconnected\n");
                is_exiting_threads = TRUE;
                return;
            }
        }
        else if (poll_result == SOCKET_ERROR)
        {
            printf("poll error for socket\n");
            return;
        }
    }
}

void SendThread(const SOCKET* socket)
{
    int retval;

    while (1)
    {
        char buf[CHATPACKET_MAX_DATA];
        fgets(buf, CHATPACKET_MAX_DATA, stdin);
        buf[strcspn(buf, "\r\n")] = 0;

        ChatPacket* packet = process_user_input(*socket, buf);
        if (is_exiting_threads == TRUE) {
            if (packet != NULL) {
                retval = send_packet(*socket, packet);

                if (retval <= 0) {
                    printf("ERROR: send (sendthread)\n");
                }
                free(packet);
            }

            print_message("Leaving room...");

            break;
        }
        retval = send_packet(*socket, packet);
        free(packet);
        if (retval <= 0) {
            printf("ERROR: send (sendthread), %d\n", WSAGetLastError());
        }
    }
}

void on_receive_packet(const ChatPacket* packet)
{
    switch (packet->type)
    {
    case MessageReceived:
        print_message(packet->data);
        break;
    default:
        break;
    }
}

void print_message(const char* data)
{
    printf("\x1b[0m");
    printf("%s\n", data);
    printf("\x1b[90m");
}

int send_packet(const SOCKET socket, const ChatPacket* packet)
{
    const char* data = serialize_packet(*packet);
    const int retval = send(socket, data, CHATPACKET_SIZE, 0);
    return retval;
}

int shutdown_socket(const SOCKET socket)
{
    const int retval = shutdown(socket, SD_BOTH);
    return retval;
}

void join_room(SOCKET socket, char* name)
{
    ChatPacket* packet = create_packet_from_string(JoinRoom, name);
    const int retval = send_packet(socket, packet);
    free(packet);
    if (retval <= 0) {
        printf("ERROR: send (join_room)\n");
        return;
    }

    current_app_state = Rooms;

    system("cls");
    printf("Connected to room \"%s\" succesfully.\n", name);

    printf("\x1b[90m");

    HANDLE threads[2];
    threads[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, (LPVOID)&socket, 0, 0);
    threads[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SendThread, (LPVOID)&socket, 0, 0);

    WaitForMultipleObjects(2, threads, TRUE, INFINITE);

    CloseHandle(threads[0]);
    CloseHandle(threads[1]);

    is_exiting_threads = FALSE;

    printf("\x1b[0m");
}