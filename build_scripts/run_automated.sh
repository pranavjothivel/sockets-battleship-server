#!/bin/bash

clear
./build_scripts/build.sh player_automated.c
if [ $? -ne 0 ]; then
  echo "Build script failed. Exiting."
  exit 1
fi

echo ""
echo "Starting automated player..."
echo ""

if [ -f "./build/player_automated" ]; then
    ./build/player_automated <argument>  # Replace <argument> with the required argument for the automated player
else
    echo "Error: ./build/player_automated does not exist. Automated player failed to start."
    exit 1
fi