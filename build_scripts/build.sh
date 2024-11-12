#!/bin/bash

mkdir -p build

sources=("hw4.c" "player_automated.c" "player_interactive.c")

if [ "$#" -gt 0 ]; then
    sources=("$@")
else
    echo "No specific files provided. Compiling all default sources."
fi

for src in "${sources[@]}"; do
    base_name=$(basename "$src" .c)
    gcc -g "./src/$src" -o "build/$base_name"
    echo "Compiled $src to build/$base_name"
done

echo "Compilation complete."