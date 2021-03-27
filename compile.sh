#!/bin/bash

mkdir -p docs
emcc game_of_life.cpp -O2 --shell-file $EMSDK/upstream/emscripten/src/shell_minimal.html -std=c++11 -s WASM=1  -o docs/index.html

