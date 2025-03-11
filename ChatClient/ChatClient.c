#include <stdio.h>
#include <winsock2.h>
#include <WS2tcpip.h>

#include "client.h"

enum ApplicationState current_app_state;

BOOL is_exiting_threads = FALSE;

char dm_recipient_name[ENTITY_NAME_MAX_LENGTH];

void flush_stdin(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

int main(void)
{
    current_app_state = Idle;
    printf("\x1b[0m");

    init_winsock();

    int retval = 0;
    ADDRINFO* server_addrinfo = NULL;

    while (TRUE)
    {
        printf("Welcome to chat!\nEnter server ip:\n");
        char server_ip[INET6_ADDRSTRLEN];
        fgets(server_ip, INET6_ADDRSTRLEN, stdin);
        server_ip[strcspn(server_ip, "\r\n")] = 0;

        ADDRINFO hints;

        set_hints(&hints);

        retval = get_addrinfo(&hints, server_ip, &server_addrinfo);
        if (retval == 0) {
            break;
        }
        system("cls");
    }

    printf("Connecting to the server...\n");

    const SOCKET socket = create_socket(server_addrinfo);

    retval = connect_to_server(socket, server_addrinfo);
    if (retval != 0) {
        printf("ERROR: connect_to_server\n");
        return -1;
    }

    printf("Connected.\n");

    while (TRUE)
    {
        printf("Enter your nickname:\n");
        char user_nickname[ENTITY_NAME_MAX_LENGTH];
        fgets(user_nickname, ENTITY_NAME_MAX_LENGTH, stdin);
        user_nickname[strcspn(user_nickname, "\r\n")] = 0;
        printf("Authorizing as \"%s\"...\n", user_nickname);

        retval = authorize_user(socket, user_nickname);
        if (retval != 0) {
            printf("Failed to authorize\n");
        }
        else
            break;
    }

    printf("Welcome to the server!\n");

    while (TRUE) {
        printf("Enter 1 if you want to join the room, or 2 if you want to send direct message to someone, or 0 to exit:\n");

        int selection = 0;
        scanf("%d", &selection);
        flush_stdin();

        if (selection == 0) {
            break;
        }

        if (selection == 1) {
            // get rooms
            // select room
            join_room(socket, "default");
            system("cls");
        }
        else if (selection == 2) {
            printf("Enter a nickname: \n");
            char recipient_name[ENTITY_NAME_MAX_LENGTH];
            fgets(recipient_name, ENTITY_NAME_MAX_LENGTH, stdin);
            recipient_name[strcspn(recipient_name, "\r\n")] = 0;
            start_dm_session(socket, recipient_name);
            system("cls");
        }
    }

    printf("Disconnecting...");

    retval = shutdown_socket(socket);
    if (retval != 0) {
        printf("ERROR: shutdown");
        return -1;
    }

    return 0;
}