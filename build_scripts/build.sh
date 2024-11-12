#!/bin/bash

# Create the build directory if it doesn't exist
mkdir -p build

# Array of source files
sources=("hw4.c" "player_automated.c" "player_interactive.c")

# Check if specific files were provided as arguments
if [ "$#" -gt 0 ]; then
    # Use command-line arguments as the source files
    sources=("$@")
else
    echo "No specific files provided. Compiling all default sources."
fi

# Loop through each source file and compile it
for src in "${sources[@]}"; do
    # Extract the base filename without extension
    base_name=$(basename "$src" .c)
    # Compile the file with debugging information, outputting to the build directory
    gcc -g "./src/$src" -o "build/$base_name"
    echo "Compiled $src to build/$base_name"
done

echo "Compilation complete."