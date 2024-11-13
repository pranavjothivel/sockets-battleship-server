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

#define SHAPES_NUM 7
#define SHAPES_ROTATIONS 4
#define SHAPES_ROWS 4
#define SHAPES_COLS 4

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
Board* fill_board_with_0s(Board *board);
bool is_piece_out_of_bounds(Board *board, Piece *piece);
bool are_ships_overlapping(Board *board, Piece *pieces);
int remaining_pieces_on_board(Board *board);

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
                        fill_board_with_0s(board01);
                        fill_board_with_0s(board02);
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
        
        while (player_01->play == true) {
            game_process_player_play_packets(buffer, 1);
        }
        
        while (player_02->play == true) {
            game_process_player_play_packets(buffer, 2);
        }
        
        break;
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

void game_process_player_board_initialize(char *buffer, int player_number) {
    Player *player = player_number == 1 ? player_01 : player_02;
    Player *other_player = player_number == 1 ? player_02 : player_01;

    read_from_player_socket(player->socket->connection_fd, buffer);

    char *token = get_first_token_from_buffer(buffer);
    if (token == NULL) {
        pstderr("game_process_player_board_initialize(): Failed to get token from buffer.");
        send_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS);
        return;
    }

    Piece* pieces = malloc(MAX_PIECES * sizeof(Piece));
    if (pieces == NULL) {
        pstderr("game_process_player_board_initialize(): malloc for 'pieces' FAILED.");
        free(token);
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
                    send_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS);
                    free(token);
                    free(pieces);
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
                send_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS);
                break;
            }

            if (count != MAX_PIECES) {
                pstderr("game_process_player_board_initialize(): Invalid initialize parameters!");
                send_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_TYPE_INVALID_PARAMETERS);
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
                        send_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHAPE_OUT_OF_RANGE);
                        free(token);
                        free(pieces);
                        return;
                    } 
                }

                for (int i = 0; i < MAX_PIECES; i++) {
                    // error 301
                    if (pieces[i].rotation < 1 || pieces[i].rotation > 4) {
                        pstderr("E 301 for rotation %d for piece %d", pieces[i].rotation, i);
                        send_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_ROTATION_OUT_OF_RANGE);
                        free(token);
                        free(pieces);
                        return;
                    }
                }

                for (int i = 0; i < MAX_PIECES; i++) {
                    // error 302
                    if (is_piece_out_of_bounds(player->board, &pieces[i])) {
                        pstderr("E 302 for piece %d", i);
                        send_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIP_DOES_NOT_FIT);
                        free(token);
                        free(pieces);
                        return;
                    }
                }

                // error 303
                if (are_ships_overlapping(player->board, pieces)) {
                        pstderr("E 303 error");
                        send_response(player->socket->connection_fd, INVALID_INITIALIZE_PACKET_SHIPS_OVERLAP);
                        free(token);
                        free(pieces);
                        return;
                    }

                // if we reach this point - there are no errors! :) 
                
                for (int i = 0; i < MAX_PIECES; i++) {
                    // fill pieces into board
                }

                send_response(player->socket->connection_fd, ACK);
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
            send_response(player->socket->connection_fd, INVALID_PACKET_TYPE_EXPECTED_INITIALIZE);
            break;
    }

    free(token);
    free(pieces);
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
                if (shoot_row >= other_player->board->height || shoot_col >= other_player->board->width) {
                    send_response(player->socket->connection_fd, INVALID_SHOOT_PACKET_CELL_OUT_OF_BOUNDS);
                    break;
                }
                else if (other_player->board->board[shoot_row][shoot_col] == -1) {
                   send_response(player->socket->connection_fd, INVALID_SHOOT_PACKET_CELL_ALREADY_GUESSED);
                   break; 
                }
                else {
                    char hit_or_miss = other_player->board->board[shoot_row][shoot_col] > 0 ? 'H' : 'M';
                    other_player->board->board[shoot_row][shoot_col] == -1;
                    int remaining_ships = remaining_pieces_on_board(other_player->board);
                    send_shot_response(player->socket->connection_fd, remaining_ships, hit_or_miss);
                    other_player->ready = true;
                    player->ready = false;
                }
            }
            else {
                send_response(player->socket->connection_fd, INVALID_SHOOT_PACKET_TYPE_INVALID_PARAMETERS);
                break;
            }

            break;
        case 'Q':
            // querying logic
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
    }

    return board;
}

Board* fill_board_with_0s(Board *board) {
    if (board == NULL) {
        pstderr("fill_board_with_0s(): board is NULL!");
        return NULL;
    }

    for (int i = 0; i < board->height; i++) {
        for (int j = 0; j < board->width; j++) {
            board->board[i][j] = 0;
        }
    }

    return board;
}

void fill_board_with_pieces(Board *board, Piece *pieces) {
    // assuming valid set of pieces as this function is only called once after error-checking
    if (board == NULL || board->board == NULL || pieces == NULL) {
        pstderr("fill_board_with_pieces(): board or pieces is NULL!");
        return;
    }

    for (int p_idx = 0; p_idx < MAX_PIECES; p_idx++) {
        int shape[SHAPES_ROWS][SHAPES_COLS];
        // memcpy(shape, tetris_shape[pieces[p_idx].type - 1][pieces[p_idx].rotation -1], sizeof(shape));

        // for (int i = 0; i < b)
    }
}

bool is_piece_out_of_bounds(Board *board, Piece *piece) {
    if (board == NULL || board->board == NULL || piece == NULL) {
        pstderr("is_shape_out_of_bounds(): board or piece is NULL!");
        return true;
    }

    int shape[SHAPES_ROWS][SHAPES_COLS];
    // minus 1 because 'type' and 'rotation' are zero-indexed
    // memcpy(shape, tetris_shape[piece->type - 1][piece->rotation - 1], sizeof(shape));

    for (int i = 0; i < SHAPES_ROWS; i++) {
        for (int j = 0; j < SHAPES_COLS; j++) {
            if (shape[i][j] != 0) {
                int row = piece->row + i;
                int col = piece->col + j;

                if (row < 0 || row >= board->height || col < 0 || col >= board->width) {
                    return true; 
                }
            }
        }
    }
    return false;
}

bool are_ships_overlapping(Board *board, Piece *pieces) {
    if (board == NULL || board->board == NULL || pieces == NULL) {
        pstderr("are_ships_overlapping(): board or pieces is NULL!");
        return true;
    }

    Board *board_cpy = create_board(board->width, board->height);
    fill_board_with_0s(board_cpy);

    for (int p = 0; p < MAX_PIECES; p++) {
        int shape[SHAPES_ROWS][SHAPES_COLS];
        // memcpy(shape, tetris_shape[pieces[p].type - 1][pieces[p].rotation - 1], sizeof(shape));

        for (int i = 0; i < SHAPES_ROWS; i++) {
            for (int j = 0; j < SHAPES_COLS; j++) {
                if (shape[i][j] != 0) {
                    int row = pieces[p].row + i;
                    int col = pieces[p].col + j;

                    if (row < 0 || row >= board->height || col < 0 || col >= board->width) {
                        pstderr("are_ships_overlapping(): Piece goes out of board bounds.");
        
                        delete_board(board_cpy);
                        return true;
                    }

                    if (board_cpy->board[row][col] != 0) {
                        pstderr("are_ships_overlapping(): Overlap detected at (%d, %d)", row, col);
                        
                        delete_board(board_cpy);
                        return true;
                    }

                    board_cpy->board[row][col] = shape[i][j];
                }
            }
        }
    }

    delete_board(board_cpy);
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

bool is_position_out_of_bounds_on_board(Board *board, int row, int col, int final_row, int final_col) {
    if (board == NULL || board->board == NULL) {
        pstderr("is_position_out_of_bounds_on_board(): board is NULL!");
        return true;
    }
    if (row < 0 || col < 0 || final_row < 0 || final_row < 0 || final_col < 0) {
        pstderr("is_position_out_of_bounds_on_board(): an argument is negative!");
        return true;
    }
    int end_row = row + final_row;
    int end_col = col + final_col;

    return (end_row >= board->height || end_col >= board->width);
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
