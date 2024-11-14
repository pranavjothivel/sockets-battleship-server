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
#define MAX_PIECES 5

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
    bool play;
    Board *board;
    PlayerSocketConnection *socket;
} Player;

// format: <Piece_type Piece_rotation Piece_column Piece_row>
typedef struct Piece {
    // ranges from 1 to 7
    int type;
    // ranges from 1 to 4
    int rotation;
    int row;
    int col;
} Piece;

// Function declarations

void read_from_player_socket(int socket_fd, char *buffer);
char* get_first_token_from_buffer(const char *buffer);
Player* initialize_player(int number, bool ready);
void delete_player(Player *player);
bool is_player_ready(Player *player);
Board* create_board(int width, int height);
bool delete_board(Board *board);
void pstdout(const char *format, ...);
void pstderr(const char *format, ...);
void send_response(int conn_fd, const char *error);
void send_shot_response(int conn_fd, int remaining_ships, const char miss_or_hit);
void end_game(void);
PlayerSocketConnection* initialize_socket_connection(int port);
int accept_player_connection(PlayerSocketConnection *player_socket);
void game_process_player_board_initialize(char *buffer, int player_number);
void print_board(Board *board);
void game_process_player_play_packets(char *buffer, int player_number);
bool are_ships_overlapping(Board *board, Piece *pieces);
int remaining_pieces_on_board(Board *board);
bool is_position_out_of_bounds_on_board(Board *board, int row, int col, int final_row, int final_col);
void fill_board_with_pieces(Board *board, Piece *pieces);
bool fill_board_with_piece(Board *board, Piece *piece, int piece_board_identifier);
bool fill_board_index_with_value(Board *board, int piece_row, int piece_col, int row_offset, int col_offset, int value);
bool is_board_index_empty(Board *board, int row, int col);
char *get_game_state_from_board(Board *board);
void send_query_response(Player* player, Board *board);

// player pointers

Player *player_01 = NULL;
Player *player_02 = NULL;

int main() {
    // register end_game() to be called at program exit - no need to manually call end_game now
    atexit(end_game);

    // ********************* Begin Server Setup ***************************
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
                        Board *board01 = create_board(width, height);
                        Board *board02 = create_board(width, height);
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
        
        if (is_player_ready(player_01) && is_player_ready(player_02)) {
            pstdout("Both Players are Ready!");
        }

        while (player_01->board->initialized == false) {
            game_process_player_board_initialize(buffer, 1);
        }

        while (player_02->board->initialized == false) {
            game_process_player_board_initialize(buffer, 2);
        }

        if (player_01->board->initialized == true && player_02->board->initialized == true) {
            pstdout("Both Players have initialized valid boards!");
            pstdout("Player 01 will now begin playing! Have fun!");
            print_board(player_01->board);
            print_board(player_02->board);
            pstdout("Printed boards!");
            player_01->play = true;
        }
        
        while (true) {
            while (player_01->play == true) {
                pstdout("Main Game Loop: Player 01 is playing!");
                game_process_player_play_packets(buffer, 1);
            }
            
            while (player_02->play == true) {
                pstdout("Main Game Loop: Player 02 is playing!");
                game_process_player_play_packets(buffer, 2);
            }
            player_01->play = true;
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

    if ((player_socket->listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        pstderr("initialize_socket(): Socket for Player FAILED.");
        free(player_socket);
        return NULL;
    }

    int opt = 1;
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
        pstderr("send_shot_response(): 'miss_or_hit' input '%c' is invalid!", miss_or_hit);
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
        pstdout("Socket read error.");
        exit(EXIT_FAILURE);
    }
    else {
        pstdout("read_from_player_socket(): Received: %s", buffer);
    }
}

void send_initialize_board_response(int conn_fd, const char *error_packet, char *token, Piece *pieces) {
    send_response(conn_fd, error_packet);
    if (token != NULL) {
        free(token);
    }
    if (pieces != NULL) {
        free(pieces);
    }
}

void game_process_player_board_initialize(char *buffer, int player_number) {
    Player *player = player_number == 1 ? player_01 : player_02;
    Player *other_player = player_number == 1 ? player_02 : player_01;

    read_from_player_socket(player->socket->connection_fd, buffer);

    char *token = get_first_token_from_buffer(buffer);
    if (token == NULL) {
        pstderr("game_process_player_board_initialize(): Failed to get token from buffer.");
        send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS, token, NULL);
        return;
    }

    Piece* pieces = malloc(MAX_PIECES * sizeof(Piece));
    if (pieces == NULL) {
        pstderr("game_process_player_board_initialize(): malloc for 'pieces' FAILED.");
        send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS, token, pieces);
        exit(EXIT_FAILURE);
    }

    int count = 0;
    int type, rotation, col, row;
    switch (*token) {
        case 'I':
            buffer += 2;

            while (sscanf(buffer, "%d %d %d %d", &type, &rotation, &col, &row) == 4) {
                if (count >= MAX_PIECES) {
                    pstderr("game_process_player_board_initialize(): Too many pieces!");
                    send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS, token, pieces);
                    return;
                }

                pieces[count].type = type;
                pieces[count].rotation = rotation;
                pieces[count].col = col;
                pieces[count].row = row;

                count++;

                // Move buffer pointer to the next set of integers
                for (int i = 0; i < 4; i++) {
                    while (*buffer && *buffer != ' ') {
                        buffer++;
                    }
                    while (*buffer == ' ') {
                        buffer++;
                    }
                }
            }

            pstdout("Count for Player %d is %d", player_number, count);
            
            int extraneous_parameter;
            if (sscanf(buffer, "%d", &extraneous_parameter) == 1) {
                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS, token, pieces);
                break;
            }

            if (count != MAX_PIECES) {
                pstderr("game_process_player_board_initialize(): Invalid initialize parameters!");
                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS, token, pieces);
            } 
            else {
                pstdout("game_process_player_board_initialize(): valid initialize parameters, checking for errors 300-303...");

                for (int i = 0; i < MAX_PIECES; i++) {
                    pstdout("Piece %d: Type=%d, Rotation=%d, Col=%d, Row=%d", i, pieces[i].type, pieces[i].rotation, pieces[i].col, pieces[i].row);
                }

                // run each check in its own for loop because we want to make sure that we return the lowest error code

                for (int i = 0; i < MAX_PIECES; i++) {
                    // error 300
                    if (pieces[i].type < 1 || pieces[i].type > 7) {
                        pstderr("E 300 for type %d for piece %d", pieces[i].type, i);
                        send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHAPE_OUT_OF_RANGE, token, pieces);
                        return;
                    } 
                }

                for (int i = 0; i < MAX_PIECES; i++) {
                    // error 301
                    if (pieces[i].rotation < 1 || pieces[i].rotation > 4) {
                        pstderr("E 301 for rotation %d for piece %d", pieces[i].rotation, i);
                        send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_ROTATION_OUT_OF_RANGE, token, pieces);
                        return;
                    }
                }

                // error 302
                for (int i = 0; i < MAX_PIECES; i++) {
                    Piece *piece = &pieces[i];
                    int piece_type = piece->type;
                    int piece_rotation = piece->rotation;
                    
                    // shape 1 - all rotations
                    if (piece_type == 1) {
                        if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                            is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                            is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 0) ||
                            is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 1)) {
                            send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                            return;
                        }   
                    }
                    // shape 2
                    else if (piece_type == 2) {
                        // rotations 1 and 3
                        if (piece_rotation == 1 || piece_rotation == 3) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 2, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 3, 0)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotations 2 and 4
                        else if (piece_rotation == 2 || piece_rotation == 4) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 2) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 3)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }  
                    }
                    // shape 3
                    else if (piece_type == 3) {
                        // rotations 1 and 3
                        if (piece_rotation == 1 || piece_rotation == 3) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, -1, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, -1, 2)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotations 2 and 4
                        else if (piece_rotation== 2 || piece_rotation == 4) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 2, 1)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                    }
                    // shape 4
                    else if (piece_type == 4) {
                        // rotation 1
                        if (piece_rotation == 1) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 2, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 2, 1)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotation 2
                        else if (piece_rotation == 2) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 2) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 0)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotation 3
                        else if (piece_rotation == 3) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 2, 1)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotation 4
                        else if (piece_rotation == 4) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 2) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, -1, 2)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                    }
                    // shape 5
                    else if (piece_type == 5) {
                        // rotations 1 and 3
                        if (piece_rotation == 1 || piece_rotation == 3) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 2)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotations 2 and 4
                        else if (piece_rotation == 2 || piece_rotation == 4) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, -1, 1)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                    }
                    // shape 6
                    else if (piece_type == 6) {
                        // rotation 1
                        if (piece_rotation == 1) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, -1, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, -2, 1)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotation 2
                        else if (piece_rotation == 2) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 2)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotation 3
                        else if (piece_rotation == 3) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 2, 0)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotation 4
                        else if (piece_rotation == 4) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 2) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 2)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                    }
                    // shape 7
                    else if (piece_type == 7) {
                        // rotation 1
                        if (piece_rotation == 1) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 2) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 1)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotation 2
                        else if (piece_rotation == 2) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, -1, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 1)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotation 3
                        else if (piece_rotation == 3) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 1) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 2) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, -1, 1)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                        // rotation 4
                        else if (piece_rotation == 4) {
                            if (is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 0, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 2, 0) ||
                                is_position_out_of_bounds_on_board(player->board, piece->row, piece->col, 1, 1)) {
                                send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT, token, pieces);
                                return;
                            }
                        }
                    }
                }

                // error 303
                if (are_ships_overlapping(player->board, pieces) == true) {
                    send_initialize_board_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIPS_OVERLAP, token, pieces);
                    return;
                }

                // if we reach this point - there are no errors! :) 
                
                fill_board_with_pieces(player->board, pieces);

                send_initialize_board_response(player->socket->connection_fd, ACK, token, pieces);
                player->board->initialized = true;
            }
            break;
        case 'F':
            send_response(player->socket->connection_fd, HALT_LOSS);
            send_response(other_player->socket->connection_fd, HALT_WIN);
            free(token);
            free(pieces);
            exit(EXIT_SUCCESS);
        default:
            send_initialize_board_response(player->socket->connection_fd, INVALID_PACKET_TYPE_EXPECTED_INITIALIZE, token, pieces);
            break;
    }
}

// Packets include Shoot, Query, and Forfeit
void game_process_player_play_packets(char *buffer, int player_number) {
    Player *player = player_number == 1 ? player_01 : player_02;
    Player *other_player = player_number == 1 ? player_02 : player_01;

    read_from_player_socket(player->socket->connection_fd, buffer);

    char *token = get_first_token_from_buffer(buffer);

    int shoot_row, shoot_col, extraneous_input;

    switch (*token) {
        case 'S':
            if (sscanf(buffer, "S %d %d %d", &shoot_row, &shoot_col, &extraneous_input) == 2) {
                // codes 400 and 401, and shooting logic
                if (is_position_out_of_bounds_on_board(player->board, shoot_row, shoot_col, 0, 0)) {
                    send_response(player->socket->connection_fd, INVALID_SHOOT_PACKET_CELL_OUT_OF_BOUNDS);
                    break;
                }
                else if (other_player->board->board[shoot_row][shoot_col] < 0) {
                    send_response(player->socket->connection_fd, INVALID_SHOOT_PACKET_CELL_ALREADY_GUESSED);
                    break; 
                }
                else {
                    char hit_or_miss = other_player->board->board[shoot_row][shoot_col] > 0 ? 'H' : 'M';
                    if (other_player->board->board[shoot_row][shoot_col] > 0) {
                        other_player->board->board[shoot_row][shoot_col] = -2;
                    } 
                    else {
                        other_player->board->board[shoot_row][shoot_col] = -1;
                    }

                    int remaining_ships = remaining_pieces_on_board(other_player->board);

                    if (remaining_ships == 0) {
                        send_shot_response(player->socket->connection_fd, remaining_ships, hit_or_miss);
                        read_from_player_socket(other_player->socket->connection_fd, buffer);
                        send_response(player->socket->connection_fd, HALT_WIN);
                        send_response(other_player->socket->connection_fd, HALT_LOSS);
                        free(token);
                        exit(EXIT_SUCCESS);
                    }

                    send_shot_response(player->socket->connection_fd, remaining_ships, hit_or_miss);
                    other_player->play = true;
                    player->play = false;
                }
            }
            else {
                send_response(player->socket->connection_fd, INVALID_SHOOT_PACKET_TYPE_INVALID_PARAMETERS);
                break;
            }

            break;
        case 'Q':
            send_query_response(player, other_player->board);
            break;
        case 'F':
            send_response(player->socket->connection_fd, HALT_LOSS);
            send_response(other_player->socket->connection_fd, HALT_WIN);
            free(token);
            exit(EXIT_SUCCESS);
        default:
            send_response(player->socket->connection_fd, INVALID_PACKET_TYPE_EXPECTED_SHOOT_QUERY_PACKET);
            break;
    }

    free(token);
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
    player->play = false;
    player->board = NULL;
    player->socket = NULL;

    return player;
}

void delete_player(Player *player) {
    if (player != NULL) {
        if (player->board != NULL) {
            pstdout("Deleting board for Player %d", player->number);
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
Board* create_board(int width, int height) {
    Board* board = malloc(sizeof(Board));

    if (board == NULL) {
        pstderr("create_board(): Error malloc'ing board.");
        return NULL;
    }

    board->pieces_remaining = 5;
    board->width = width;
    board->height = height;
    board->initialized = false;

    board->board = malloc(board->height * sizeof(int*));

    if (board->board == NULL) {
        pstderr("create_board(): Error malloc'ing rows for board.");
        free(board);
        return NULL;
    }

    for (unsigned int i = 0; i < board->height; i++) {
        board->board[i] = malloc(board->width * sizeof(int));
        if (board->board[i] == NULL) {
            pstderr("create_board(): Error malloc'ing cols for a row in board.");
            
            for (unsigned int j = 0; j < i; j++) {
                free(board->board[j]);
            }
            free(board->board);
            free(board); // Free the allocated board structure

            return NULL;
        }

        memset(board->board[i], 0, board->width * sizeof(int)); // this 
    }

    // using memset instead of this
    // for (int i = 0; i < board->height; i++) {
    //     for (int j = 0; j < board->width; j++) {
    //         board->board[i][j] = 0;
    //     }
    // }

    return board;
}

void fill_board_with_pieces(Board *board, Piece *pieces) {
    // assuming valid set of pieces as this function is only called once after error-checking
    if (board == NULL || board->board == NULL || pieces == NULL) {
        pstderr("fill_board_with_pieces(): board or pieces is NULL!");
        return;
    }

    for (int p_idx = 0; p_idx < MAX_PIECES; p_idx++) {
        Piece *piece = &pieces[p_idx];
        fill_board_with_piece(board, piece, p_idx + 1);
    }
}

bool fill_board_with_piece(Board *board, Piece *piece, int piece_board_identifier) {
    if (board == NULL || board->board == NULL) {
        pstderr("fill_board_with_piece(): board or pieces is NULL!");
        return false;
    }
    if (piece == NULL) {
        pstderr("fill_board_with_piece(): board or pieces is NULL!");
        return false;
    }

    int piece_type = piece->type;
    int piece_rotation = piece->rotation;
    // shape 1 - all rotations
    if (piece_type == 1) {
        bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                            fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                            fill_board_index_with_value(board, piece->row, piece->col, 1, 0, piece_board_identifier) &&
                            fill_board_index_with_value(board, piece->row, piece->col, 1, 1, piece_board_identifier);
        if (!fill_success) {
            return false;
        }
    }
    // shape 2
    else if (piece_type == 2) {
        // rotations 1 and 3
        if (piece_rotation == 1 || piece_rotation == 3) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 2, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 3, 0, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotations 2 and 4
        else if (piece_rotation == 2 || piece_rotation == 4) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 2, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 3, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
    }
    // shape 3
    else if (piece_type == 3) {
        // rotations 1 and 3
        if (piece_rotation == 1 || piece_rotation == 3) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, -1, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, -1, 2, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotations 2 and 4
        else if (piece_rotation == 2 || piece_rotation == 4) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 2, 1, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
    }
    // shape 4
    else if (piece_type == 4) {
        // rotation 1
        if (piece_rotation == 1) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 2, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 2, 1, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotation 2
        else if (piece_rotation == 2) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 2, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 0, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotation 3
        else if (piece_rotation == 3) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 2, 1, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotation 4
        else if (piece_rotation == 4) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 2, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, -1, 2, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
    }
    // shape 5
    else if (piece_type == 5) {
        // rotations 1 and 3
        if (piece_rotation == 1 || piece_rotation == 3) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 2, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotations 2 and 4
        else if (piece_rotation == 2 || piece_rotation == 4) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, -1, 1, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
    }
    // shape 6
    else if (piece_type == 6) {
        // rotation 1
        if (piece_rotation == 1) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, -1, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, -2, 1, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotation 2
        else if (piece_rotation == 2) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 2, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotation 3
        else if (piece_rotation == 3) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 2, 0, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotation 4
        else if (piece_rotation == 4) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 2, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 2, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
    }
    // shape 7
    else if (piece_type == 7) {
        // rotation 1
        if (piece_rotation == 1) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 2, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 1, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotation 2
        else if (piece_rotation == 2) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, -1, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 1, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotation 3
        else if (piece_rotation == 3) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 1, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 0, 2, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, -1, 1, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
        // rotation 4
        else if (piece_rotation == 4) {
            bool fill_success = fill_board_index_with_value(board, piece->row, piece->col, 0, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 2, 0, piece_board_identifier) &&
                                fill_board_index_with_value(board, piece->row, piece->col, 1, 1, piece_board_identifier);
            if (!fill_success) {
                return false;
            }
        }
    }

    return true;
}

bool fill_board_index_with_value(Board *board, int piece_row, int piece_col, int row_offset, int col_offset, int value) {
    if (board == NULL || board->board == NULL) {
        pstderr("fill_board_index_with_value(): board is NULL!");
        return false;
    }

    int row = piece_row + row_offset;
    int col = piece_col + col_offset;

    if (is_position_out_of_bounds_on_board(board, row, col, 0, 0)) {
        pstderr("fill_board_index_with_value(): FAILED at (%d, %d) - Out of bounds!", row, col);
        return false;
    }
    if (!is_board_index_empty(board, row, col)) {
        pstderr("fill_board_index_with_value(): FAILED at (%d, %d) - Not 0!", row, col);
        return false;
    }
    board->board[row][col] = value;
    return true;
}

bool is_board_index_empty(Board *board, int row, int col) {
    if (board == NULL || board->board == NULL) {
        pstderr("fill_board_index_with_value(): board is NULL!");
        return false;
    }
    if (is_position_out_of_bounds_on_board(board, row, col, 0, 0)) {
        pstderr("is_board_index_empty(): FAILED at (%d, %d) - Out of bounds!", row, col);
        return false;
    }
    return board->board[row][col] == 0;
}

bool are_ships_overlapping(Board *board, Piece *pieces) {
    if (board == NULL || board->board == NULL || pieces == NULL) {
        pstderr("are_ships_overlapping(): board or pieces is NULL!");
        return true;
    }
    Board *board_cpy = create_board(board->width, board->height);
    for (int p_idx = 0; p_idx < MAX_PIECES; p_idx++) {
        Piece *piece = &pieces[p_idx];
        int piece_board_identifier = p_idx + 1;
        int piece_type = piece->type;
        int piece_rotation = piece->rotation;

        if (!fill_board_with_piece(board_cpy, piece, piece_board_identifier)) {
            delete_board(board_cpy);
            pstdout("are_ships_overlapping(): fill_board_with_piece() failed and deleting 'board_cpy'!");
            return true;
        }
    }

    delete_board(board_cpy);
    pstdout("are_ships_overlapping(): no overlapping ships and deleting 'board_cpy'!");
    return false;
}

void print_board(Board *board) {
    if (board == NULL || board->board == NULL) {
        pstderr("print_board(): board is NULL!");
        return;
    }

    pstdout("Board (%d x %d):", board->width, board->height);
    for (int i = 0; i < board->height; i++) {
        for (int j = 0; j < board->width; j++) {
            printf("%d ", board->board[i][j]);
        }
        printf("\n");
    }
}

bool delete_board(Board *board) {
    if (board == NULL || board->board == NULL) {
        pstdout("delete_board(): board is NULL.");
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
    free(board);

    return true; // Indicate successful deletion
}

bool board_has_ship(Board *board, int piece_number) {
    if (board ==  NULL || board->board == NULL) {
        pstderr("board_has_ship(): board is NULL!");
        return false;
    }
    if (piece_number < 1 || piece_number > 5) {
        pstderr("board_has_ship(): invalid 'piece_number'!");
        return false;
    }

    for (int i = 0; i < board->height; i++) {
        for (int j = 0; j < board->width; j++) {
            if (board->board[i][j] == piece_number) {
                return true;
            }
        }
    }
    return false;
}

int remaining_pieces_on_board(Board *board) {
    if (board == NULL || board->board == NULL) {
        pstderr("remaining_pieces_on_board(): board is NULL!");
        return -1;
    }
    int counter = 0;

    for (int i = 1; i <= MAX_PIECES; i++) {
        if (board_has_ship(board, i) == true) {
            counter++;
        }
    }
    return counter;
}

void send_query_response(Player* player, Board *board) {
    char *state = get_game_state_from_board(board);
    send_response(player->socket->connection_fd, state);
    free(state);
}

bool is_position_out_of_bounds_on_board(Board *board, int piece_row_idx, int piece_col_idx, int new_row_offset, int new_col_offset) {
    if (board == NULL || board->board == NULL) {
        pstderr("is_position_out_of_bounds_on_board(): board is NULL!");
        return true;
    }

    if (piece_row_idx < 0 || piece_col_idx < 0) {
        pstderr("is_position_out_of_bounds_on_board(): piece row or piece column is NULL, therefore trivally out of bounds!");
        return true;
    }

    int new_row_idx = piece_row_idx + new_row_offset;
    int new_col_idx = piece_col_idx + new_col_offset;

    return !(new_row_idx >= 0 && new_col_idx >= 0 && new_row_idx < board->height && new_col_idx < board->width);


}

void insert_pieces_by_shape_rotation_into_board(Board *board, Piece *pieces) {
    if (board == NULL || board->board == NULL) {
        pstderr("insert_shape_rotation_into_board(): board is NULL!");
        return;
    }
    if (pieces == NULL) {
        pstderr("insert_shape_rotation_into_board(): 'pieces' is NULL!");
        return;
    }

    // piece type and rotations have already been validated

    for (int p_idx = 0; p_idx < MAX_PIECES; p_idx++) {
        int piece_identifier = p_idx + 1;
        Piece *piece = &pieces[p_idx];

        if (piece->type == 1) {
            board->board[piece->row][piece->col] = piece_identifier;
            board->board[piece->row][piece->col + 1] = piece_identifier;
            board->board[piece->row + 1][piece->col] = piece_identifier;
            board->board[piece->row + 1 ][piece->col + 1] = piece_identifier;
        }
        else if (piece->type == 2) {
            if (piece->rotation == 1 || piece->rotation == 3) {
                board->board[piece->row][piece->col] = piece_identifier;
                board->board[piece->row + 1][piece->col] = piece_identifier;
                board->board[piece->row + 2][piece->col] = piece_identifier;
                board->board[piece->row + 3][piece->col] = piece_identifier;
            }
            else if (piece->rotation == 2 || piece->rotation == 4) {
                board->board[piece->row][piece->col] = piece_identifier;
                board->board[piece->row][piece->col + 1] = piece_identifier;
                board->board[piece->row][piece->col + 2] = piece_identifier;
                board->board[piece->row][piece->col + 3] = piece_identifier;
            }
        }
    }
}

char *get_game_state_from_board(Board *board) {
    char *buffer = malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        pstderr("get_game_state_from_board(): malloc failed!");
        return NULL;
    }

    buffer[0] = '\0';
    strcat(buffer, "G");

    char temp[BUFFER_SIZE];

    int remaining_pieces = remaining_pieces_on_board(board);
    snprintf(temp, sizeof(temp), " %d", remaining_pieces);
    strncat(buffer, temp, BUFFER_SIZE - strlen(buffer) - 1);

    for (int i = 0; i < board->height; i++) {
        for (int j = 0; j < board->width; j++) {
            int idx = board->board[i][j];
            if (idx < 0) {
                // miss
                if (idx == -1) {
                    snprintf(temp, sizeof(temp), " M %d %d", j, i);
                } 
                // hit
                else if (idx == -2) {
                    snprintf(temp, sizeof(temp), " H %d %d", j, i);
                }
                strncat(buffer, temp, BUFFER_SIZE - strlen(buffer) - 1);
            }
        }
    }

    return buffer;
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
    pstdout("Ending game...");
    if (player_01 != NULL) {
        delete_player(player_01);
        player_01 = NULL;
    }
    if (player_02 != NULL) {
        delete_player(player_02);
        player_02 = NULL;
    }
}
