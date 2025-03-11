#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "26067"

#define CHATPACKET_SIZE 1028
#define CHATPACKET_MAX_DATA 1024
#define ENTITY_NAME_MAX_LENGTH 50

#define CONNECTION_TIMEOUT 60000

enum ApplicationState
{
    Idle,
    Rooms,
    DMs
};

extern enum ApplicationState current_app_state;
extern char dm_recipient_name[ENTITY_NAME_MAX_LENGTH];
extern BOOL is_exiting_threads;

enum ChatPacketType
{
    Authorize,
    JoinRoom,
    LeaveRoom,
    MessageRoom,
    MessagePrivate,
    MessageReceived = 1000,
    AuthorizeResult
};

typedef struct _ChatPacket
{
    int type;
    char data[CHATPACKET_MAX_DATA]; //1024
} ChatPacket;

ChatPacket* create_packet_from_int(int type, int n);
ChatPacket* create_packet_from_string(int type, const char* str);
ChatPacket* create_packet_from_full_string(const int type, const char* str);

char* serialize_packet(ChatPacket packet);
ChatPacket* deserialize_packet(const char* source);

int init_winsock(void);
void set_hints(ADDRINFO* hints);
int get_addrinfo(const ADDRINFO* hints, const char* server_ip, ADDRINFO** result);
SOCKET create_socket(const ADDRINFO* result);
int connect_to_server(SOCKET socket, const ADDRINFO* addrinfo);

int authorize_user(SOCKET socket, const char* nickname);

void start_dm_session(SOCKET socket, char* recipient_name);
ChatPacket* process_user_input(SOCKET socket, const char* input);
char* format_dm_data(const char* input, const char* recepient);

void ReceiveThread(const SOCKET* socket);
void SendThread(const SOCKET* socket);

void on_receive_packet(const ChatPacket* packet);
void print_message(const char* data);
int send_packet(SOCKET socket, const ChatPacket* packet);
int shutdown_socket(SOCKET socket);
void join_room(SOCKET socket, char* name);