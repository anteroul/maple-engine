#ifndef MAPLEDOOM_DOOMAUDIO_H
#define MAPLEDOOM_DOOMAUDIO_H

#include "Game.h"
#include "Audio/Audio.h"
#include <array>

/// Every sound in the game, synthesized at start up: no audio files needed.
struct DoomSounds {
    /// By doom::Sound. The monster voices are kept apart, below.
    std::array<SoundClipPtr, (int)doom::Sound::Count> effects;
    /// By monster, then alert, pain and death.
    std::array<std::array<SoundClipPtr, 3>, 4> voices;
    /// A dark, looping riff.
    SoundClipPtr music;

    /// The clip to play for a sound the game made.
    const SoundClipPtr& clipFor(const doom::SoundEvent& event) const;
};

DoomSounds createDoomSounds();

#endif //MAPLEDOOM_DOOMAUDIO_H
