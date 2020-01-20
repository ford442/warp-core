#include <stdio.h>
#include <emscripten.h>

#include "input.h"

InputState input;

extern "C" {
        
    void EMSCRIPTEN_KEEPALIVE initInput() {
        input.left = input.right = input.up = input.down = input.mouseClick = 0;
        input.x = input.y = -1;
    }

    void EMSCRIPTEN_KEEPALIVE processKey(int key, int state) {
        switch (key) {
            case 87: // W
            case 38: // ArrowUp
                input.up = state;
            break;
            case 83: // S
            case 40: // ArrowDown
                input.down = state;
            break;
            case 65: // A
            case 37: // ArrowLeft
                input.left = state;
            break;
            case 68: // D
            case 39: // ArrowRight
                input.right = state;
            break;
            


        }
    }

    void EMSCRIPTEN_KEEPALIVE processMouseMovement(int x, int y) {
    }

}