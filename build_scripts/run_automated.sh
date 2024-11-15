#!/bin/bash

clear

if [ -z "$1" ]; then
    echo "Error: No argument provided."
    exit 1
fi

if [ ! -f "$1" ]; then
    echo "Error: Argument file '$1' not found."
    exit 1
fi

./build_scripts/build.sh player_automated.c
if [ $? -ne 0 ]; then
    echo "Build script failed. Exiting."
    exit 1
fi

echo ""
echo "Starting automated player..."
echo ""

echo "Running: ./build/player_automated $1"
echo ""

if [ -f "./build/player_automated" ]; then
    ./build/player_automated "$1"
else
    echo "Error: ./build/player_automated does not exist. Automated player failed to start."
    exit 1
fi