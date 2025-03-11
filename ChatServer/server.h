#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define CHATPACKET_SIZE 1028
#define CHATPACKET_MAX_DATA 1024
#define ENTITY_NAME_MAX_LENGTH 50

#define CONNECTION_TIMEOUT 60000

#include <Windows.h>
#include <WinSock2.h>

// Users

typedef struct _Activity
{
    char id[ENTITY_NAME_MAX_LENGTH];
    int type;
    struct _Activity* next;
} Activity;

Activity* activity_create(const char* id, int type);
void activity_free(Activity* activity);

typedef struct _User
{
    SOCKET socket;
    char nickname[ENTITY_NAME_MAX_LENGTH];
    Activity* activity;
} User;

void end_user_activities(const User* user);
void user_free(User* user);

typedef struct _UserList
{
    User** head;
    int count;
    int capacity;
} UserList;

extern UserList* global_user_list;

UserList* userlist_create(void);
void userlist_free(UserList* ul);
void userlist_add(UserList* ul, User* user);
void userlist_remove(UserList* ul, const char* nickname);
User* userlist_find(const UserList* ul, const char* nickname);

// Rooms

typedef struct _Room
{
    char name[ENTITY_NAME_MAX_LENGTH];
    UserList* users;
} Room;

Room* room_create(const char* name);
void room_free(Room* room);

typedef struct _RoomList
{
    Room* head;
    int count;
    int capacity;
} RoomList;

extern RoomList* global_room_list;

RoomList* roomlist_create(void);
void roomlist_add(RoomList* rl, Room room);
void roomlist_create_room(RoomList* rl, const char* name);
void roomlist_remove(RoomList* rl, const char* name);
Room* roomlist_find(const RoomList* rl, const char* name);

// Chat packet

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
    char data[CHATPACKET_MAX_DATA];
} ChatPacket;

ChatPacket* create_packet_from_int(int type, int n);
ChatPacket* create_packet_from_string(int type, const char* str);

char* serialize_packet(ChatPacket packet);
ChatPacket* deserialize_packet(const char* source);

void enqueue_send(const User* user, ChatPacket packet);

void process_packet(ChatPacket packet, User* user);

// User control

BOOL authorize_user(User* user, const char* nickname);

void room_announce(const Room* room, const char* announcement);
void room_publish_user_message(const Room* room, User* from, const char* message);
void room_join(const Room* room, User* user);
void room_leave(const Room* room, const char* nickname);

void send_direct_message(const User* to, User* from, char* message);