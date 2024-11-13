#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
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
    bool initialized;
    int width;
    int height;
    int **board;
} Board;

typedef struct PlayerSocketConnection {
    int connection_fd;
    int listen_fd;
    struct sockaddr_in address;
    socklen_t address_len;
    int port;
} PlayerSocketConnection;

typedef struct Player {
    int number;
    bool ready;
    Board *board;
    PlayerSocketConnection *socket;
} Player;


// Function declarations

void read_from_player_socket(int socket_fd, char *buffer);
char* get_first_token_from_buffer(const char *buffer);
Player* initialize_player(int number, bool ready);
void delete_player(Player *player);
bool is_player_ready(Player *player);
Board* initialize_board(int width, int height);
bool delete_board(Board *board);
void pstdout(const char *format, ...);
void pstderr(const char *format, ...);
void send_response(int conn_fd, const char *error);
void send_shot_response(int conn_fd, int remaining_ships, const char miss_or_hit);
void end_game(void);
PlayerSocketConnection* initialize_socket_connection(int port);
int accept_player_connection(PlayerSocketConnection *player_socket);

// player pointers

Player *player_01 = NULL;
Player *player_02 = NULL;

int main() {
    // register end_game() to be called at program exit - no need to manually call end_game now
    atexit(end_game);

    // ********************* Begin Server Setup ***************************
    // DO NOT TOUCH SOCKET SETUP!!
    // Game server setup on ports 2201 and 2202

    char buffer[BUFFER_SIZE] = {0};

    player_01 = initialize_player(1, false);
    player_02 = initialize_player(2, false);

    player_01->socket = initialize_socket_connection(PLAYER01_PORT);
    player_02->socket = initialize_socket_connection(PLAYER02_PORT);

    if (player_01->socket == NULL || player_02->socket == NULL) {
        pstderr("Failed to initialize player sockets.");
        exit(EXIT_FAILURE);
    }

    pstdout("Waiting for players to connect...");

    if (accept_player_connection(player_01->socket) < 0) {
        pstderr("[Server] Player 01: accept() failed.");
        exit(EXIT_FAILURE);
    }
    pstdout("Player 01: accept() success.");

    if (accept_player_connection(player_02->socket) < 0) {
        pstderr("[Server] Player 02: accept() failed.");
        exit(EXIT_FAILURE);
    }
    pstdout("Player 02: accept() success.");

    pstdout("Ready to play Battleship!");

    // ***************************** End Server Setup ***********************************

    // ************************** Server -> Main Game Loop **********************************
    while (true) {
        while (is_player_ready(player_01) == false) {
            read_from_player_socket(player_01->socket->connection_fd, buffer);
            
            char* token = get_first_token_from_buffer(buffer);
            int width, height, extraneous_input;
            switch (*token) {
                case 'B':
                    if (sscanf(buffer, "B %d %d %d", &width, &height, &extraneous_input) == 2 && width >= 10 && height >= 10) {
                        Board *board01 = initialize_board(width, height);
                        Board *board02 = initialize_board(width, height);
                        player_01->board = board01;
                        player_02->board = board02;

                        player_01->ready = true;
                        pstdout("Player 01 is ready to begin!");
                        send_response(player_01->socket->connection_fd, ACK);
                    }
                    else {
                        send_response(player_01->socket->connection_fd, INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS);
                    }
                    break;
                case 'F':
                    send_response(player_01->socket->connection_fd, HALT_LOSS);
                    send_response(player_02->socket->connection_fd, HALT_WIN);
                    free(token);
                    exit(EXIT_SUCCESS);
                default:
                    send_response(player_01->socket->connection_fd, INVALID_PACKET_TYPE_EXPECTED_BEGIN);
                    break;
            }
            free(token);
        }

        while (is_player_ready(player_02) == false) {
            read_from_player_socket(player_02->socket->connection_fd, buffer);
            
            char *token = get_first_token_from_buffer(buffer);
            int extraneous_input;

            switch (*token) {
                case 'B':
                    if (sscanf(buffer, "B %d", &extraneous_input) == 1) {
                        send_response(player_02->socket->connection_fd, INVALID_BEGIN_PACKET_TYPE_INVALID_PARAMETERS);
                        break;
                    }
                    else {
                        player_02->ready = true;
                        pstdout("Player 02 is ready to begin!");
                        send_response(player_02->socket->connection_fd, ACK);
                        break;
                    }
                case 'F':
                    send_response(player_02->socket->connection_fd, HALT_LOSS);
                    send_response(player_01->socket->connection_fd, HALT_WIN);
                    free(token);
                    exit(EXIT_SUCCESS);
                default:
                    send_response(player_02->socket->connection_fd, INVALID_PACKET_TYPE_EXPECTED_BEGIN);
                    break;
            }
            free(token);
        }
        
        // main game logic goes here with board, rotations, and everything 
        if (is_player_ready(player_01) && is_player_ready(player_02)) {
            pstdout("Both Players are Ready!");
            break;
        }
    }

    return EXIT_SUCCESS;
}

PlayerSocketConnection* initialize_socket_connection(int port) {
    PlayerSocketConnection* player_socket = malloc(sizeof(PlayerSocketConnection));

    if (player_socket == NULL) {
        pstderr("initialize_socket(): Error malloc'ing socket");
        return NULL;
    }

    int opt = 1;

    if ((player_socket->listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        pstderr("initialize_socket(): Socket for Player FAILED.");
        free(player_socket);
        return NULL;
    }

    if (setsockopt(player_socket->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        pstderr("initialize_socket(): setsockopt(..., SO_REUSEADDR, ...) failed for a player port.");
        free(player_socket);
        return NULL;
    }
    // no need to use so_reuseport as per Piazza post

    player_socket->port = port;

    player_socket->address.sin_family = AF_INET;
    player_socket->address.sin_addr.s_addr = INADDR_ANY;
    player_socket->address.sin_port = htons(port);

    player_socket->address_len = sizeof(player_socket->address);

    if (bind(player_socket->listen_fd, (struct sockaddr *)&player_socket->address, player_socket->address_len) < 0) {
        pstderr("initialize_socket(): bind failed.");
        free(player_socket);
        return NULL;
    }

    if (listen(player_socket->listen_fd, 3) < 0) {
        pstderr("initialize_socket(): listen failed.");
        free(player_socket);
        return NULL;
    }
    
    return player_socket;
}

int accept_player_connection(PlayerSocketConnection *player_socket) {
    player_socket->connection_fd = accept(player_socket->listen_fd, (struct sockaddr *)&player_socket->address, &player_socket->address_len);
    return player_socket->connection_fd;
}

void send_response(int conn_fd, const char *packet) {
    send(conn_fd, packet, strlen(packet), 0);
}

void send_shot_response(int conn_fd, int remaining_ships, const char miss_or_hit) {
    if (miss_or_hit != 'M' && miss_or_hit != 'H') {
        pstderr("send_shot_response(): 'miss_or_hit' input is invalid!");
        return;
    }
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "R %d %c", remaining_ships, miss_or_hit);
    send(conn_fd, response, strlen(response), 0);
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

// token is a pointer, need to 'free' it
char* get_first_token_from_buffer(const char *buffer) {
    char *buffer_cpy = strdup(buffer);
    if (buffer_cpy == NULL) {
        return NULL;
    }

    char *token = strtok(buffer_cpy, " ");
    if (token == NULL) {
        free(buffer_cpy);
        return NULL;
    }

    char *result = strdup(token);
    
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
    player->socket = NULL;

    return player;
}

void delete_player(Player *player) {
    if (player != NULL) {
        if (player->board != NULL) {
            delete_board(player->board);
            player->board = NULL;
        }

        if (player->socket != NULL) {
            close(player->socket->connection_fd);
            close(player->socket->listen_fd);
            free(player->socket);
            player->socket = NULL;
        }

        free(player);
        player = NULL;
    }
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
    board->initialized = false;

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
    board->initialized = false;

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

void end_game(void) {
    if (player_01 != NULL) {
        delete_player(player_01);
        player_01 = NULL;
    }
    if (player_02 != NULL) {
        delete_player(player_02);
        player_02 = NULL;
    }
}
