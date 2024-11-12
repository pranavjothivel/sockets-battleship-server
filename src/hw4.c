#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <asm-generic/socket.h>

#define PLAYER01_PORT 2201
#define PLAYER02_PORT 2202
#define BUFFER_SIZE 1024

// game error codes
#define INVALID_PACKET_TYPE_EXPECTED_BEGIN "E 100"
#define INVALID_PACKET_TYPE_EXPECTED_INITIALIZE "E 101"
#define INVALID_PACKET_TYPE_EXPECTED_SHOOT_QUERY_PACKET "E 102"

#define INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS "E 200"
#define INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS "E 201"
#define INVALID_SHOOT_PACKET_TYPE_INVALID_PARAMETERS "E 202"

#define INVALID_INITIALIZE_PACKET_SHAPE_OUT_OF_RANGE "E 300"
#define INVALID_INITIALIZE_PACKET_ROTATION_OUT_OF_RANGE "E 301"
#define INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT "E 302"
#define INVALID_INITIALIZE_PACKET_SHIPS_OVERLAP "E 303"

#define INVALID_SHOOT_PACKET_CELL_OUT_OF_BOUNDS "E 400"
#define INVALID_SHOOT_PACKET_CELL_ALREADY_GUESSED "E 401"

bool player_01_ready = false;
bool player_02_ready = false;

/* Function Declarations */

void send_response(int conn_fd, const char *error);
void read_from_player_socket(int socket_fd, char *buffer);
void end_game(int *conn_fd_01, int *conn_fd_02, int *listen_fd_01, int *listen_fd_02);

typedef struct Board {
    int pieces_remaining;
    int **board;
} Board;


// TODO: later, make player sockets into their own structs
// typedef struct PlayerSocketConnection {
//     int connection_fd;
//     int listen_fd;
//     struct sockaddr_in address;
//     int address_len;
//     int port;
// } PlayerSocketConnection;

typedef struct Player {
    int number;
    bool ready;
    Board board;
    //PlayerSocketConnection socket;
} Player;

int main() {
    // Game server setup on ports 2201 and 2202
    int conn_fd_01, conn_fd_02;
    int listen_fd_01, listen_fd_02;
    struct sockaddr_in address_01, address_02;
    int opt = 1;
    int address_01_len = sizeof(address_01);
    int address_02_len = sizeof(address_02);

    char buffer[BUFFER_SIZE] = {0};

    if ((listen_fd_01 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("[Server] Socket for Player 01 FAILED.\n");
        exit(EXIT_FAILURE);
    }
    if ((listen_fd_02 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("[Server] Socket for Player 02 FAILED.\n");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_fd_01, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ||
        setsockopt(listen_fd_02, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[Server] setsockopt(..., SO_REUSEADDR, ...) failed for a player port.\n");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(listen_fd_01, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) ||
        setsockopt(listen_fd_02, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("[Server] setsockopt(..., SO_REUSEPORT, ...) failed for a player port.\n");
        exit(EXIT_FAILURE);
    }

    // zero out address pointers
    memset(&address_01, 0, sizeof(address_01));
    memset(&address_02, 0, sizeof(address_02));

    address_01.sin_family = AF_INET;
    address_01.sin_addr.s_addr = INADDR_ANY;
    address_01.sin_port = htons(PLAYER01_PORT);

    address_02.sin_family = AF_INET;
    address_02.sin_addr.s_addr = INADDR_ANY;
    address_02.sin_port = htons(PLAYER02_PORT);

    // bind sockets
    if (bind(listen_fd_01, (struct sockaddr *)&address_01, sizeof(address_01)) < 0) {
        perror("[Server] Player 01 : bind() failed.\n");
        exit(EXIT_FAILURE);
    }
    if (bind(listen_fd_02, (struct sockaddr *)&address_02, sizeof(address_02)) < 0) {
        perror("[Server] Player 02 : bind() failed.\n");
        exit(EXIT_FAILURE);
    }

    // listen for connections
    if (listen(listen_fd_01, 3) < 0) {
        perror("[Server] Player 01 : listen() failed.\n");
        exit(EXIT_FAILURE);
    }
    if (listen(listen_fd_02, 3) < 0) {
        perror("[Server] Player 02 : listen() failed.\n");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Waiting for players to connect...\n");

    conn_fd_01 = accept(listen_fd_01, (struct sockaddr *)&address_01, (socklen_t*)&address_01_len);
    if (conn_fd_01 < 0) {
        perror("[Server] Player 01: accept() failed.\n");
        end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 01: accept() success.\n");

    conn_fd_02 = accept(listen_fd_02, (struct sockaddr *)&address_02, (socklen_t*)&address_02_len);
    if (conn_fd_02 < 0) {
        perror("[Server] Player 02: accept() failed.\n");
        end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 02: accept() success.\n");

    printf("[Server] Ready to play Battleship!\n");

    // End Server Setup **************************************************************

    // Server -> Main Game Loop

    while (true) {
        while (!player_01_ready) {
            read_from_player_socket(conn_fd_01, buffer);

            char packet_type;
            int width, height;
            int extra_input;

            if (sscanf(buffer, "%c %d %d %d", &packet_type, &width, &height, &extra_input) == 4 && packet_type == 'B') {
                send_response(conn_fd_01, INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS);
            } 
            else if (sscanf(buffer, "%c %d %d", &packet_type, &width, &height) == 3 && packet_type == 'B') {
                if (width >= 10 && height >= 10) {
                    player_01_ready = true;
                    send_response(conn_fd_01, "A");
                } else {
                    send_response(conn_fd_01, INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS);
                }
            } 
            else if (sscanf(buffer, "%c %d", &packet_type, &width) == 2 && packet_type == 'B') {
                send_response(conn_fd_01, INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS);
            }
            else if (sscanf(buffer, "%c", &packet_type) == 1 && packet_type == 'B') {
                send_response(conn_fd_01, INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS);
            }  
            else {
                send_response(conn_fd_01, INVALID_PACKET_TYPE_EXPECTED_BEGIN);
            }
        }

        while (!player_02_ready) {
            read_from_player_socket(conn_fd_02, buffer);

            char packet_type;
            char extra_char;

            if (sscanf(buffer, "%c %c", &packet_type, &extra_char) == 1 && packet_type == 'B') {
                player_02_ready = true;
                send_response(conn_fd_02, "A");
            } else {
                send_response(conn_fd_02, INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS);
            }
        }

        if (player_01_ready && player_02_ready) {
            printf("Both Players are Ready!\n");
            break;
        }
    }


    end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
    return EXIT_SUCCESS;
}

void send_response(int conn_fd, const char *error) {
    send(conn_fd, error, strlen(error), 0);
}

void read_from_player_socket(int socket_fd, char *buffer) {
    memset(buffer, 0, BUFFER_SIZE);

    int nbytes = read(socket_fd, buffer, BUFFER_SIZE);
    if (nbytes <= 0) {
        printf("[Server] Socket read error.\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    else {
        printf("[Server] Received: %s\n", buffer);
    }
}

void end_game(int *conn_fd_01, int *conn_fd_02, int *listen_fd_01, int *listen_fd_02) {
    close(*conn_fd_01);
    close(*conn_fd_02);
    close(*listen_fd_01);
    close(*listen_fd_02);
}