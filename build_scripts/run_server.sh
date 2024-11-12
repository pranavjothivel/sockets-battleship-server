#!/bin/bash

clear
# Run the build script
./build_scripts/build.sh
if [ $? -ne 0 ]; then
  echo "Build script failed. Exiting."
  exit 1
fi

# # Compile hw4.c
# gcc -o ./build/hw4 ./build/hw4.c
# if [ $? -ne 0 ]; then
#   echo "Compilation of hw4.c failed. Exiting."
#   exit 1
# fi
echo ""
echo "Starting server..."
echo ""

# Run the compiled executable
./build/hw4