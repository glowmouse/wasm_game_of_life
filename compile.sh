#!/bin/bash

mkdir -p docs
emcc game_of_life.cpp -O2 --shell-file $EMSDK/upstream/emscripten/src/shell_minimal.html -std=c++11 -s WASM=1 -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 -o docs/index.html

