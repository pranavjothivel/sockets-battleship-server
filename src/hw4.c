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

bool player_01_ready = false;
bool player_02_ready = false;

// function declarations
void end_game(int *conn_fd_01, int *conn_fd_02, int *listen_fd_01, int *listen_fd_02);

typedef struct Board {
    int pieces_remaining;
    int **board;
} Board;

typedef struct Player {
    int number;
    bool ready;
    Board board;
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

    while (1) {
        conn_fd_01 = accept(listen_fd_01, (struct sockaddr *)&address_01, (socklen_t*)&address_01_len);
        if (conn_fd_01 < 0) {
            perror("[Server] Player 01: accept() failed.\n");
            exit(EXIT_FAILURE);
        } else {
            printf("[Server] Player 01: accept() success.\n");
        }

        conn_fd_02 = accept(listen_fd_02, (struct sockaddr *)&address_02, (socklen_t*)&address_02_len);
        if (conn_fd_02 < 0) {
            perror("[Server] Player 02: accept() failed.\n");
            exit(EXIT_FAILURE);
        } else {
            printf("[Server] Player 02: accept() success.\n");
        }

        memset(buffer, 0, sizeof(buffer));

        while (!player_01_ready && !player_02_ready) {
            
        }

    }

    end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
    return EXIT_SUCCESS;
}

void end_game(int *conn_fd_01, int *conn_fd_02, int *listen_fd_01, int *listen_fd_02) {
    close(*conn_fd_01);
    close(*conn_fd_02);
    close(*listen_fd_01);
    close(*listen_fd_02);
}