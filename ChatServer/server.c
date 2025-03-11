#include "server.h"

#include <stdio.h>
#include <stdlib.h>

// Activity

enum ActivityType
{
    RoomActivity
};

Activity* activity_create(const char* id, const int type)
{
    Activity* activity = (Activity*)malloc(sizeof(Activity));
    ZeroMemory(activity->id, ENTITY_NAME_MAX_LENGTH);
    strncpy(activity->id, id, min(strlen(id), ENTITY_NAME_MAX_LENGTH));
    activity->type = type;
    activity->next = NULL;
    return activity;
}

void activity_free(Activity* activity)
{
    if (activity == NULL)
        return;

    free(activity);
}

// User

void end_user_activities(const User* user)
{
    if (user->activity == NULL)
        return;

    if(user->activity->type == RoomActivity)
    {
        const Room* room = roomlist_find(global_room_list, user->activity->id);
        room_leave(room, user->nickname);
    }
    free(user->activity);
}

void user_free(User* user)
{
    activity_free(user->activity);
    free(user);
}

// User list

UserList* global_user_list;

UserList* userlist_create(void)
{
    UserList* ul = (UserList*)malloc(sizeof(UserList));

    ul->head = (User**)malloc(sizeof(User*));
    ul->capacity = 4;
    ul->count = 0;

    return ul;
}

void userlist_free(UserList* ul)
{
    free(ul->head);
    free(ul);
}

void userlist_add(UserList* ul, User* user)
{
    if (ul->count + 1 > ul->capacity)
    {
        ul->head = (User**)realloc(ul->head, sizeof(User*) * ul->capacity * 2);
        ul->capacity *= 2;
    }

    ul->head[ul->count++] = user;
}

void userlist_remove(UserList* ul, const char* nickname)
{
    for (int i = 0; i < ul->count; i++)
    {
        if (strcmp(ul->head[i]->nickname, nickname) == 0)
        {
            memcpy(ul->head + i, ul->head + i + 1, (ul->count - 1 - i) * sizeof(User*));
            ul->count--;
            return;
        }
    }
}

User* userlist_find(const UserList* ul, const char* nickname)
{
    if (strcmp(nickname, "") == 0)
        return NULL;

    for (int i = 0; i < ul->count; i++)
        if (strcmp(ul->head[i]->nickname, nickname) == 0)
            return ul->head[i];

    return NULL;
}

// Rooms

RoomList* global_room_list;

Room* room_create(const char* name)
{
    Room* room = (Room*)malloc(sizeof(Room));

    ZeroMemory(room->name, ENTITY_NAME_MAX_LENGTH);
    strncpy(room->name, name, min(strlen(name), ENTITY_NAME_MAX_LENGTH));

    room->users = userlist_create();

    return room;
}

void room_free(Room* room)
{
    userlist_free(room->users);
    free(room);
}

RoomList* roomlist_create(void)
{
    RoomList* ul = (RoomList*)malloc(sizeof(RoomList));

    ul->head = (Room*)malloc(sizeof(Room));
    ul->capacity = 1;
    ul->count = 0;

    return ul;
}

void roomlist_add(RoomList* rl, const Room room)
{
    if (rl->count + 1 > rl->capacity)
    {
        rl->head = (Room*)realloc(rl->head, sizeof(Room) * rl->capacity * 2);
        rl->capacity *= 2;
    }

    rl->head[rl->count++] = room;
}

void roomlist_create_room(RoomList* rl, const char* name)
{
    Room room = { 0 };
    ZeroMemory(room.name, ENTITY_NAME_MAX_LENGTH);
    strncpy(room.name, name, min(strlen(name), ENTITY_NAME_MAX_LENGTH));
    room.users = userlist_create();
    roomlist_add(rl, room);
}

void roomlist_remove(RoomList* rl, const char* name)
{
    for (int i = 0; i < rl->count; i++)
    {
        if (strcmp(rl->head[i].name, name) == 0)
        {
            userlist_free(rl->head[i].users);
            memcpy(rl->head + i, rl->head + i + 1, (rl->count - 1 - i) * sizeof(Room));
            rl->count--;
            return;
        }
    }
}

Room* roomlist_find(const RoomList* rl, const char* name)
{
    for (int i = 0; i < rl->count; i++)
        if (strcmp(rl->head[i].name, name) == 0)
            return &rl->head[i];

    return NULL;
}

// Chat packet

ChatPacket* create_packet_from_int(const int type, const int n)
{
    ChatPacket* packet = (ChatPacket*)malloc(sizeof(ChatPacket));
    packet->type = type;
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

void enqueue_send(const User* user, const ChatPacket packet)
{
    char* serialized = serialize_packet(packet);
    int r = send(user->socket, serialized, CHATPACKET_SIZE, 0);
    free(serialized);
}

void process_packet(const ChatPacket packet, User* user)
{
    switch (packet.type)
    {
    case Authorize:
        // set user nickname from data
        const BOOL result = authorize_user(user, packet.data);
        ChatPacket* return_packet = create_packet_from_int(AuthorizeResult, result);
        enqueue_send(user, *return_packet);
        free(return_packet);
        break;
    case JoinRoom:
        // join room with name from data
        const Room* joining_room = roomlist_find(global_room_list, packet.data);
        room_join(joining_room, user);
        break;
    case LeaveRoom:
        // leave current room
        const Room* leaving_room = roomlist_find(global_room_list, user->activity->id);
        room_leave(leaving_room, user->nickname);
        break;
    case MessageRoom:
        // send message to current room
        const Room* messaging_room = roomlist_find(global_room_list, user->activity->id);
        room_publish_user_message(messaging_room, user, packet.data);
        break;
    case MessagePrivate:
        // send private message to user with name
        char recipient[50];
        ZeroMemory(recipient, ENTITY_NAME_MAX_LENGTH);
        strncpy(recipient, packet.data, ENTITY_NAME_MAX_LENGTH);
        char message[CHATPACKET_MAX_DATA - ENTITY_NAME_MAX_LENGTH];
        ZeroMemory(message, CHATPACKET_MAX_DATA - ENTITY_NAME_MAX_LENGTH);
        strncpy(message, &packet.data[ENTITY_NAME_MAX_LENGTH], CHATPACKET_MAX_DATA - ENTITY_NAME_MAX_LENGTH);
        const User* recipient_user = userlist_find(global_user_list, recipient);
        if (recipient_user == NULL)
            return;
        send_direct_message(recipient_user, user, message);
        break;
    default:
        // unknown packet type
        printf("received unknown packet");
        break;
    }
}

// User control & messaging

BOOL authorize_user(User* user, const char* nickname)
{
    if (userlist_find(global_user_list, nickname) != NULL)
        return FALSE;

    ZeroMemory(user->nickname, ENTITY_NAME_MAX_LENGTH);
    strncpy(user->nickname, nickname, min(strlen(nickname), ENTITY_NAME_MAX_LENGTH));
    return TRUE;
}

void room_announce(const Room* room, const char* announcement)
{
    for (int i = 0; i < room->users->count; i++)
    {
        ChatPacket* packet = create_packet_from_string(MessageReceived, announcement);
        enqueue_send(room->users->head[i], *packet);
        free(packet);
    }
}

void room_publish_user_message(const Room* room, User* from, const char* message)
{
    char buffer[CHATPACKET_MAX_DATA];
    sprintf(buffer, "<%s>: %s", from->nickname, message);
    room_announce(room, buffer);
}

void room_join(const Room* room, User* user)
{
    userlist_add(room->users, user);
    user->activity = activity_create(room->name, RoomActivity);

    char buffer[CHATPACKET_MAX_DATA];
    sprintf(buffer, "User <%s> joined", user->nickname);
    room_announce(room, buffer);
}

void room_leave(const Room* room, const char* nickname)
{
    userlist_remove(room->users, nickname);

    char buffer[CHATPACKET_MAX_DATA];
    sprintf(buffer, "User <%s> left", nickname);
    room_announce(room, buffer);
}

void send_direct_message(const User* to, User* from, char* message)
{
    char buffer[CHATPACKET_MAX_DATA];
    sprintf(buffer, "[%s]: %s", from->nickname, message);
    ChatPacket* packet = create_packet_from_string(MessageReceived, buffer);
    enqueue_send(to, *packet);
    free(packet);
}