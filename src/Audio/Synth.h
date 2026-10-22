#ifndef MAPLEENGINE_SYNTH_H
#define MAPLEENGINE_SYNTH_H

#include "Audio.h"
#include <vector>

/// A small chiptune synthesizer, so games can have sound effects and music
/// without shipping audio files.
namespace synth {

constexpr unsigned int DEFAULT_SAMPLE_RATE = 44100;

enum class Waveform {
    Square,
    Triangle,
    Sine,
    Sawtooth,
    /// White noise. Frequency sets how often a new random value is drawn,
    /// so lower frequencies sound rougher.
    Noise
};

/// One synthesized sound: a waveform that slides from one frequency to
/// another under a simple attack / release envelope.
struct Tone {
    Waveform waveform = Waveform::Square;
    /// Frequency at the start and the end of the tone, in Hz. The slide
    /// between them is exponential, so it sounds even across octaves.
    float startFrequency = 440.f;
    float endFrequency = 440.f;
    /// Length in seconds, including the release.
    float duration = 0.1f;
    float volume = 1.f;
    /// Seconds to fade in and to fade out. A few milliseconds of each stops
    /// the tone from clicking at its edges.
    float attack = 0.005f;
    float release = 0.02f;
    /// Fraction of each cycle a square wave spends high. 0.5 is hollow,
    /// 0.125 thin and nasal.
    float dutyCycle = 0.5f;
};

/// Render tones one after another into a clip.
SoundClip render(const std::vector<Tone>& tones, unsigned int sampleRate = DEFAULT_SAMPLE_RATE);

/// One note of a melody.
struct Note {
    /// MIDI note number, 69 is A4 at 440 Hz. REST for silence.
    int pitch;
    /// Length in beats.
    float beats;
};

constexpr int REST = -1;

/// Frequency of a MIDI note number, in Hz.
float noteFrequency(int pitch);

/// Render a melody. Every note uses `instrument` for its waveform, volume and
/// envelope; its frequency and length come from the note.
SoundClip renderMelody(const std::vector<Note>& notes, float beatsPerMinute, const Tone& instrument,
                       unsigned int sampleRate = DEFAULT_SAMPLE_RATE);

/// Sum clips sample by sample, for layering a melody over a bass line. The
/// result is as long as the longest clip. All clips must share a rate.
SoundClip mix(const std::vector<SoundClip>& clips);

/// Turn a clip into something Audio can play, and that can be shared.
inline SoundClipPtr share(SoundClip clip)
{
    return std::make_shared<const SoundClip>(std::move(clip));
}

} // namespace synth

#endif //MAPLEENGINE_SYNTH_H
