# CSE 220 - Homework 01 - Sockets Battleship Server  
**Pranav Jothivel**

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

## 3. How to play

### a. Automatically starting server and clients
There are two pre-configured Tasks called `Run Server and Automated Players` and `Run Server and Interactive Players` in Visual Studio Code for you to use. 
- `Run Server and Automated Players` will start the game server and prompt you to enter one of the scripts located in `scripts/` in the format of `scripts/<file_name>`. The launch task will then open three terminal tabs side-by-side for each player and the server.
- `Run Server and Interactive Players` will start the game server and open three terminal tabs for each player along with one for the player.

Once this is done, you can start playing!

### b. Manually launch server and clients

To manually launch the server and automated clients, follow the below steps.

1. Go to `Terminal` → `New Terminal` → `Split Terminal` (repeat twice - this is located to the top-right of the terminal).
2. Run `./build_scripts/run_server.sh` in one terminal tab to start the server.
3. Run `./build_scripts/run_automated.sh scripts/<file_name>` in each of the two remaining terminal tabs to start the automated player clients.

To manually launch the server and interactive clients, follow the below steps.

1. Go to `Terminal` → `New Terminal` → `Split Terminal` (repeat twice - this is located to the top-right of the terminal).
2. Run `./build_scripts/run_server.sh` in one terminal tab to start the server.
3. Run `./build_scripts/run_interactive.sh` in each of the two remaining terminal tabs to start the interactive player clients.

Once you have started the server and two clients, please enter the corresponding player number (`1` or `2`) in each client to begin playing!

## 4. Rules of the game
There are five valid commands/packets a player can send to the server during the game.

1. The Begin (`B`) packet:

    Player 1 must 
<!-- Add comments for start playing, valid commands, using scripted testing, and valgrind and vs code launch targets -->