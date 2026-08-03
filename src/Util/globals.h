#ifndef MAPLEENGINE_GLOBALS_H
#define MAPLEENGINE_GLOBALS_H

#include "../App.h"

struct GameState {
    unsigned int level;
    bool gameOver;
    bool paused;
    bool fullscreen;
    unsigned int w, h;
    std::string resolution;
    std::string gpu;
};

static GameState state;

#endif