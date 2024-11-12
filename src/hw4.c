#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <asm-generic/socket.h>
#include <stdarg.h>
#include <ctype.h>

#define PLAYER01_PORT 2201
#define PLAYER02_PORT 2202
#define BUFFER_SIZE 1024

// server responses

// error codes
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

#define HALT_WIN "H 1"
#define HALT_LOSS "H 0"

#define ACK "A"

typedef struct Board {
    int pieces_remaining;
    int width;
    int height;
    int **board;
} Board;


// TODO: later, make player sockets into their own structs
// typedef struct PlayerSocketConnection {
//     int connection_fd;
//     int listen_fd;
//     struct sockaddr_in address;
//     socklen_t address_len;
//     int port;
// } PlayerSocketConnection;

typedef struct Player {
    int number;
    bool ready;
    Board *board;
    //PlayerSocketConnection *socket;
} Player;


// Function declarations

void read_from_player_socket(int socket_fd, char *buffer);
char get_first_token_from_buffer(const char *buffer);
Player* initialize_player(int number, bool ready);
void delete_player(Player *player);
bool is_player_ready(Player *player);
Board* initialize_board(int width, int height);
bool delete_board(Board *board);
void pstdout(const char *format, ...);
void pstderr(const char *format, ...);
void send_response(int conn_fd, const char *error);
void end_game(int *conn_fd_01, int *conn_fd_02, int *listen_fd_01, int *listen_fd_02);

// player  pointers
Player *player_01 = NULL;
Player *player_02 = NULL;

int main() {
    // ********************* Begin Server Setup ***************************
    // DO NOT TOUCH SOCKET SETUP!!
    // Game server setup on ports 2201 and 2202

    int conn_fd_01, conn_fd_02;
    int listen_fd_01, listen_fd_02;
    struct sockaddr_in address_01, address_02;
    int opt = 1;
    int address_01_len = sizeof(address_01);
    int address_02_len = sizeof(address_02);

    char buffer[BUFFER_SIZE] = {0};

    if ((listen_fd_01 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        pstderr("Socket for Player 01 FAILED.");
        exit(EXIT_FAILURE);
    }
    if ((listen_fd_02 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        pstderr("Socket for Player 02 FAILED.");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_fd_01, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ||
        setsockopt(listen_fd_02, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        pstderr("setsockopt(..., SO_REUSEADDR, ...) failed for a player port.");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(listen_fd_01, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) ||
        setsockopt(listen_fd_02, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
        pstderr("setsockopt(..., SO_REUSEPORT, ...) failed for a player port.");
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
        pstderr("Player 01 : bind() failed.\n");
        exit(EXIT_FAILURE);
    }
    if (bind(listen_fd_02, (struct sockaddr *)&address_02, sizeof(address_02)) < 0) {
        pstderr("Player 02 : bind() failed.\n");
        exit(EXIT_FAILURE);
    }

    // listen for connections
    if (listen(listen_fd_01, 3) < 0) {
        pstderr("Player 01 : listen() failed.\n");
        exit(EXIT_FAILURE);
    }
    if (listen(listen_fd_02, 3) < 0) {
        pstderr("Player 02 : listen() failed.\n");
        exit(EXIT_FAILURE);
    }

    pstdout("Waiting for players to connect...");

    conn_fd_01 = accept(listen_fd_01, (struct sockaddr *)&address_01, (socklen_t*)&address_01_len);
    if (conn_fd_01 < 0) {
        pstderr("[Server] Player 01: accept() failed.");
        end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
        exit(EXIT_FAILURE);
    }
    player_01 = initialize_player(1, false);
    pstdout("Player 01: accept() success.");

    conn_fd_02 = accept(listen_fd_02, (struct sockaddr *)&address_02, (socklen_t*)&address_02_len);
    if (conn_fd_02 < 0) {
        pstderr("[Server] Player 02: accept() failed.");
        end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
        exit(EXIT_FAILURE);
    }
    player_02 = initialize_player(2, false);
    pstdout("Player 02: accept() success.");

    pstdout("Ready to play Battleship!");

    // ***************************** End Server Setup ***********************************

    // ************************** Server -> Main Game Loop **********************************
    while (true) {
        while (is_player_ready(player_01) == false) {
            read_from_player_socket(conn_fd_01, buffer);
            
            char token = get_first_token_from_buffer(buffer);
            int width, height, extraneous_input;
            switch (token) {
                case 'B':
                    if (sscanf(buffer, "B %d %d %d", &width, &height, &extraneous_input) == 2 && width >= 10 && height >= 10) {
                        Board *board01 = initialize_board(width, height);
                        Board *board02 = initialize_board(width, height);
                        player_01->board = board01;
                        player_02->board = board02;

                        player_01->ready = true;
                        pstdout("Player 01 is ready to begin!");
                        send_response(conn_fd_01, ACK);
                        break;
                    }
                    else {
                        send_response(conn_fd_01, INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS);
                        break;
                    }
                case 'F':
                    send_response(conn_fd_01, HALT_LOSS);
                    send_response(conn_fd_02, HALT_WIN);
                    end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
                    exit(EXIT_SUCCESS);
                default:
                    send_response(conn_fd_01, INVALID_PACKET_TYPE_EXPECTED_BEGIN);
                    break;
            }
        }

        while (is_player_ready(player_02) == false) {
            read_from_player_socket(conn_fd_02, buffer);
            
            char token = get_first_token_from_buffer(buffer);
            int extraneous_input;

            switch (token) {
                case 'B':
                    if (sscanf(buffer, "B %d", &extraneous_input) == 1) {
                        send_response(conn_fd_02, INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS);
                        break;
                    }
                    else {
                        player_02->ready = true;
                        pstdout("Player 02 is ready to begin!");
                        send_response(conn_fd_02, ACK);
                        break;
                    }
                case 'F':
                    send_response(conn_fd_02, HALT_LOSS);
                    send_response(conn_fd_01, HALT_WIN);
                    end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
                    exit(EXIT_SUCCESS);
                default:
                    send_response(conn_fd_02, INVALID_PACKET_TYPE_EXPECTED_BEGIN);
                    break;
            }
        }
        
        // main game logic goes here with board, rotations, and everything 
        if (is_player_ready(player_01) && is_player_ready(player_02)) {
            pstdout("Both Players are Ready!");
            // end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
            // return EXIT_SUCCESS;
            break;
        }
    }

    end_game(&conn_fd_01, &conn_fd_02, &listen_fd_01, &listen_fd_02);
    return EXIT_SUCCESS;
}

// PlayerSocketConnection* initialize_socket(int port) {
//     PlayerSocketConnection* socket = malloc(sizeof(PlayerSocketConnection));
//     if (socket == NULL) {
//         pstderr("initialize_socket(): Error malloc'ing socket");
//     }
//     socket->port = port;
//     return socket;
// }

void send_response(int conn_fd, const char *packet) {
    send(conn_fd, packet, strlen(packet), 0);
}

void send_shot_response(int conn_fd, int remaining_ships, const char* miss_or_hit) {

}

void read_from_player_socket(int socket_fd, char *buffer) {
    memset(buffer, 0, BUFFER_SIZE);

    int nbytes = read(socket_fd, buffer, BUFFER_SIZE);
    if (nbytes <= 0) {
        pstdout("Socket read error.\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    else {
        pstdout("read_from_player_socket(): Received: %s", buffer);
    }
}

char get_first_token_from_buffer(const char *buffer) {
    char *buffer_cpy = strdup(buffer);
    if (buffer_cpy == NULL) {
        return '\0';
    }

    char *token = strtok(buffer_cpy, " ");
    if (token == NULL) {
        free(buffer_cpy);
        return '\0';
    }

    bool is_num = true;
    for (int i = 0; token[i] != '\0'; i++) {
        if (!isdigit((unsigned char)token[i])) {
            is_num = false;
            break;
        }
    }

    char result;
    if (is_num) {
        int num = atoi(token);
        if (num >= 0 && num <= 127) {
            result = (char)num;
        } 
        else {
            result = '\0';
        }
    }
    else {
        result = token[0];
    }

    free(buffer_cpy);
    return result;
}

Player* initialize_player(int number, bool ready) {
    Player* player = malloc(sizeof(Player));
    if (player == NULL) {
        pstderr("initialize_player(): Error malloc'ing player!");
        return NULL;
    }
    player->number = number;
    player->ready = ready;
    player->board = NULL;
    return player;
}

void delete_player(Player *player) {
    delete_board(player->board);
    free(player);
    player == NULL;
}

bool is_player_ready(Player *player) {
    // assume valid pointer is passed.
    return player->ready;
}

// width is the number of cols, and height is number of rows
Board* initialize_board(int width, int height) {
    Board* board = malloc(sizeof(Board));
    if (board == NULL) {
        pstderr("initialize_board(): Error malloc'ing board.");
        return NULL;
    }
    board->pieces_remaining = 5;
    board->width = width;
    board->height = height;

    board->board = malloc(board->height * sizeof(int*));
    if (board->board == NULL) {
        pstderr("initialize_board(): Error malloc'ing rows for board.");
        free(board);
        return NULL;
    }

    for (unsigned int i = 0; i < board->height; i++) {
        board->board[i] = malloc(board->width * sizeof(int));
        if (board->board[i] == NULL) {
            pstderr("initialize_board(): Error malloc'ing cols for a row in board.");
            for (unsigned int j = 0; j < i; j++) {
                free(board->board[j]);
            }
            free(board->board);
            free(board); // Free the allocated board structure
            return NULL;
        }
    }

    return board;
}

bool delete_board(Board *board) {
    if (board == NULL || board->board == NULL) {
        printf("delete_board(): board is NULL.");
        return false;
    }

    for (int i = 0; i < board->height; i++) {
        free(board->board[i]);
    }

    free(board->board);

    board->board = NULL;
    board->pieces_remaining = 0;
    board->width = 0;
    board->height = 0;

    return true; // Indicate successful deletion
}

void pstdout(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("[Server] - [INFO] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void pstderr(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[Server] - [ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void end_game(int *conn_fd_01, int *conn_fd_02, int *listen_fd_01, int *listen_fd_02) {
    // free board memory
    close(*conn_fd_01);
    close(*conn_fd_02);
    close(*listen_fd_01);
    close(*listen_fd_02);
}
