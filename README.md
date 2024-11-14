# CSE 220 - Homework 01 - Sockets Battleship Server  
**Pranav Jothivel**

## About
- **Programming Language:** C  
- **Compiler:** GCC  
- **Tools/Technologies:** C Sockets API, Valgrind  
- **Development Environment:** Debian running on GitHub Codespaces  

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

## How To Play

### 1. Start a Codespace
This project was developed using a GitHub Codespace, and it is recommended to use the same environment to avoid compatibility issues.

To start a Codespace:
1. Go to `<> Code` → `Codespaces` → `Create codespace on main`.
2. This will build a Codespace using the predefined `Dockerfile` and settings in `devcontainer.json`.

### 2. Configure and Run Build Scripts
Once your Codespace is running:
1. Run `chmod +x ./build_scripts/*.sh` to make all the build scripts executable.
2. Run `./build_scripts/build.sh` to build the Battleship server and client executables.

### 3. Start Playing