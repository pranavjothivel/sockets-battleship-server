#!/bin/bash

clear
./build_scripts/build.sh hw4.c
if [ $? -ne 0 ]; then
  echo "Build script failed. Exiting."
  exit 1
fi

echo ""
echo "Starting server..."
echo ""

if [ -f "./build/hw4" ]; then
    ./build/hw4
else
    echo "Error: ./build/hw4 does not exist. Server failed to start."
    exit 1
fi