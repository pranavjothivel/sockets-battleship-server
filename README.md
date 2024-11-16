# CSE 220 - Homework 01: Sockets Battleship Server  
**Author:** Pranav Jothivel  
**Date:** November 2024

## About
- **Programming Language:** C  
- **Compiler:** GCC  
- **Tools/Technologies:** C Sockets API, Valgrind  
- **Development Environment:** Debian 12 running on GitHub Codespaces
- **Concepts:** Network Programming, Sockets, Structs, Data Strcutures, Dynamic Memory Allocation, Pointers, Game Logic

This project is a classic implementation of the Battleship game, hosted on a server to enable gameplay between two clients. Each client connects to a designated port and communicates with the server by sending a series of valid commands to set up their boards and play. The game continues until one player successfully sinks all the opponent’s ships or a player forfeits.

## AutoTest Results
- [x] `Compiler Output`  
- [x] `P1 Forfeit`  
- [x] `P1 B bad`  
- [x] `P1 I wrong`  
- [x] `P1 S errors`  
- [x] `P1 Q`  
- [x] `P1 Win`  
- [x] `P2 Forfeit`  
- [x] `P2 B bad`  
- [x] `P2 I wrong`  
- [x] `P2 S invalid`  
- [x] `P2 Q try`  
- [x] `P2 Win`  
- [x] `Random A`  
- [x] `Random B`  
- [x] `Random C`  
- [x] `Random D`  
- [x] `Random E`  
- [x] `Random F`  
- [x] `Random G`  
- [x] `Random H`  
- [x] `Random I`  
- [x] `Random J`  
- [x] `Random K`  
- [x] `Random L`  
- [x] `Random M`  
- [x] `Random N`  
- [x] `Random O`  

## Valgrind Results
- **Memory Leak Check:** All memory leaks fixed. (`All heap blocks were freed -- no leaks are possible`)

## Setting up the environment

### 1. Start a Codespace
This project was developed using a GitHub Codespace, and it is recommended to use the same environment to avoid compatibility issues.

To start a Codespace:
1. Go to `<> Code` → `Codespaces` → `Create codespace on main`.
2. This will build a Codespace using the predefined `Dockerfile` and settings in `devcontainer.json`.

### 2. Configure and Run Build Scripts
Once your Codespace is running:
1. Run `chmod +x ./build_scripts/*.sh` to make all the build scripts executable.
2. Run `./build_scripts/build.sh` to build the Battleship server and client executables.


## Ship format
In this Battleship implementation, Tetris-style pieces are used as ships, adding a unique twist to the classic game. Each ship's shape and orientation mimic Tetris pieces, requiring players to strategically position them on the board for optimal coverage and defense.

![Tetris Pieces Diagrams](https://github.com/pranavjothivel/sockets-battleship-server/blob/main/docs/images/TetrisDiagram.drawio.jpeg?raw=true)


## Packet format

### Part 1: Received Packet Formats

The server supports five types of packets, each formatted as an ASCII-encoded string. Packet types and formats are as follows:

1. **Begin (`B`)**  
   - **Format:** `B <Width_of_board Height_of_board>`
   - **Example:** `B 11 11`
   - Player 1 specifies the board dimensions; Player 2 only sends `B` to join.

2. **Initialize (`I`)**  
   - **Format:** `I <Piece_type Piece_rotation Piece_column Piece_row>`
   - **Example:** `I 1 1 0 0 1 1 0 2 1 1 0 4 1 1 2 2 1 1 2 0`
   - Each client sends data for piece types, rotations, and positions, which are validated for placement legality (within bounds, no overlap).

3. **Shoot (`S`)**  
   - **Format:** `S <Row Column>`
   - **Example:** `S 1 1`
   - Players take turns sending shot coordinates, with the server marking hits or misses.

4. **Query (`Q`)**  
   - Clients request a history of shots and remaining ships without ending their turn.

5. **Forfeit (`F`)**  
   - A player can forfeit their turn, causing the server to halt the game for both players.

### Part 2: Response Packet Formats

Server responses include:

1. **Error (`E <Error_Code>`)**  
   - **Example:** `E 101`  
   - Server sends an error code for invalid commands (details in Part 3).

2. **Halt (`H {1 for win, 0 for loss}`)**  
   - **Example:** `H 1`  
   - Sent in response to forfeit or when the last ship is sunk.

3. **Acknowledgment (`A`)**  
   - Acknowledges valid Begin and Initialize packets.

4. **Query Response (`G <ships_remaining> <misses_and_hits>`)**  
   - **Example:** `G 5 M 0 0 H 1 1`
   - Shows past hits/misses in row-major order.

5. **Shot Response (`R <ships_remaining> <M for miss, H for hit>`)**  
   - **Example:** `R 5 M`  
   - Sends result of the last shot and remaining ship count.

### Part 3: Error Codes

1. **Packet Type Errors:**
   - `100`: Invalid packet type (Expected Begin packet)
   - `101`: Invalid packet type (Expected Initialize packet)
   - `102`: Invalid packet type (Expected Shoot, Query, or Forfeit)

2. **Packet Format Errors:**
   - `200`: Invalid Begin packet (incorrect number of parameters or parameter out of range)
   - `201`: Invalid Initialize packet (incorrect number of parameters)
   - `202`: Invalid Shoot packet (incorrect number of parameters)

3. **Initialize Packet Errors:**
   - `300`: Invalid Initialize packet (piece type out of range)
   - `301`: Invalid Initialize packet (rotation out of range)
   - `302`: Invalid Initialize packet (piece does not fit within game board)
   - `303`: Invalid Initialize packet (piece overlaps with another piece)

4. **Shoot Packet Errors:**
   - `400`: Invalid Shoot packet (coordinates out of game board bounds)
   - `401`: Invalid Shoot packet (coordinate already guessed)

## How to play

### a. Automatically starting server and clients
There are two pre-configured Tasks called `Run Server and Automated Players` and `Run Server and Interactive Players` in Visual Studio Code for you to use. 
- `Run Server and Automated Players` will start the game server and prompt you to enter one of the scripts located in `scripts/` in the format of `scripts/<file_name>`. The launch task will then open three terminal tabs side-by-side for each player and the server.
- `Run Server and Interactive Players` will start the game server and open three terminal tabs for each player along with one for the player.

### b. Manually launch server and clients

To manually launch the server and automated clients, follow the below steps.

1. Go to `Terminal` → `New Terminal` → `Split Terminal` (repeat twice - this is located to the top-right of the terminal).
2. Run `./build_scripts/run_server.sh` in one terminal tab to start the server.
3. Run `./build_scripts/run_automated.sh scripts/<file_name>` in each of the two remaining terminal tabs to start the automated player clients.

To manually launch the server and interactive clients, follow the below steps.

1. Go to `Terminal` → `New Terminal` → `Split Terminal` (repeat twice - this is located to the top-right of the terminal).
2. Run `./build_scripts/run_server.sh` in one terminal tab to start the server.
3. Run `./build_scripts/run_interactive.sh` in each of the two remaining terminal tabs to start the interactive player clients.

Once you have started the server and two clients, please enter the corresponding player number (`1` or `2`) in each client to begin playing! The game continues until a player forfeits or takes out all of their opponents' ships.

## Memory leak checking and server logs

To run the server with Valgrind:
```bash
gcc -g src/hw4.c -o ./build/hw4 > output.log 2>&1 && valgrind --leak-check=full --log-file=valgrind_output.log --show-leak-kinds=all ./build/hw4 >> output.log 2>&1
```
The server's output log is `output.log` and **Valgrind**'s log filename is `valgrind_output.log`.