#ifndef MAPLETETRIS_TETRISAUDIO_H
#define MAPLETETRIS_TETRISAUDIO_H

#include "Audio/Audio.h"
#include <array>

/// Every sound in the game, synthesized at start up: no audio files needed.
struct TetrisSounds {
    SoundClipPtr move;
    SoundClipPtr rotate;
    SoundClipPtr hold;
    SoundClipPtr lock;
    SoundClipPtr hardDrop;
    /// Indexed by lines cleared at once, minus one: single to tetris.
    std::array<SoundClipPtr, 4> lineClear;
    SoundClipPtr levelUp;
    SoundClipPtr pause;
    SoundClipPtr gameOver;
    /// Korobeiniki, the Russian folk song Tetris made famous. Public domain.
    SoundClipPtr music;
};

TetrisSounds createTetrisSounds();

#endif //MAPLETETRIS_TETRISAUDIO_H
