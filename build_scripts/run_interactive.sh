#!/bin/bash

clear
./build_scripts/build.sh player_interactive.c
if [ $? -ne 0 ]; then
  echo "Build script failed. Exiting."
  exit 1
fi

echo ""
echo "Starting player..."
echo ""

if [ -f "./build/hw4" ]; then
    ./build/player_interactive
else
    echo "Error: ./build/player_interactive does not exist. Player failed to start."
    exit 1
fi