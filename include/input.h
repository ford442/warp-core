#pragma once
#include <emscripten.h>

struct InputState {
    int left;
    int right;
    int up;
    int down;

    int x;
    int y;
    int mouseClick;
};

extern InputState input;

extern "C" {
        
    void EMSCRIPTEN_KEEPALIVE initInput();
    void EMSCRIPTEN_KEEPALIVE processKeyDown(int key);
    void EMSCRIPTEN_KEEPALIVE processKeyUp(int key);

    void EMSCRIPTEN_KEEPALIVE processMouseMovement(int x, int y);
}

