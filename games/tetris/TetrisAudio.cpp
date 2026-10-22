#include "TetrisAudio.h"
#include "Audio/Synth.h"

using namespace synth;

namespace {

/// MIDI note numbers used below.
enum : int {
    B1 = 35, C2 = 36, D2 = 38, E2 = 40, GS2 = 44, A2 = 45, B2 = 47,
    C3 = 48, D3 = 50, E3 = 52, GS3 = 56, A3 = 57,
    GS4 = 68, A4 = 69, B4 = 71, C5 = 72, D5 = 74, E5 = 76, F5 = 77, G5 = 79,
    GS5 = 80, A5 = 81, B5 = 83, C6 = 84, E6 = 88, A6 = 93
};

/// Music runs at a lower rate than effects: it is long, and the chiptune
/// waveforms lose nothing audible at 22 kHz.
constexpr unsigned int MUSIC_SAMPLE_RATE = 22050;
constexpr float MUSIC_TEMPO = 150.f;

Tone blip(Waveform waveform, float startHz, float endHz, float duration, float volume,
          float duty = 0.5f)
{
    Tone tone;
    tone.waveform = waveform;
    tone.startFrequency = startHz;
    tone.endFrequency = endHz;
    tone.duration = duration;
    tone.volume = volume;
    tone.dutyCycle = duty;
    return tone;
}

/// A rising run of notes, one after another, each `step` seconds long.
SoundClip arpeggio(const std::vector<int>& notes, float step, float volume, float duty)
{
    std::vector<Tone> tones;

    for (int note: notes)
    {
        const float frequency = noteFrequency(note);
        tones.push_back(blip(Waveform::Square, frequency, frequency, step, volume, duty));
    }

    // Let the last note ring out a little longer.
    tones.back().duration *= 2.5f;
    tones.back().release = tones.back().duration * 0.8f;
    return render(tones);
}

/// The A section and the slower B section of Korobeiniki. Each bar is four
/// beats.
std::vector<Note> melody()
{
    const std::vector<Note> partA = {
            {E5, 1}, {B4, .5f}, {C5, .5f}, {D5, 1}, {C5, .5f}, {B4, .5f},
            {A4, 1}, {A4, .5f}, {C5, .5f}, {E5, 1}, {D5, .5f}, {C5, .5f},
            {B4, 1.5f}, {C5, .5f}, {D5, 1}, {E5, 1},
            {C5, 1}, {A4, 1}, {A4, 2},
            {REST, .5f}, {D5, 1}, {F5, .5f}, {A5, 1}, {G5, .5f}, {F5, .5f},
            {E5, 1.5f}, {C5, .5f}, {E5, 1}, {D5, .5f}, {C5, .5f},
            {B4, 1}, {B4, .5f}, {C5, .5f}, {D5, 1}, {E5, 1},
            {C5, 1}, {A4, 1}, {A4, 2},
    };
    const std::vector<Note> partB = {
            {E5, 2}, {C5, 2}, {D5, 2}, {B4, 2},
            {C5, 2}, {A4, 2}, {GS4, 2}, {B4, 2},
            {E5, 2}, {C5, 2}, {D5, 2}, {B4, 2},
            {C5, 1}, {E5, 1}, {A5, 2}, {GS5, 4},
    };

    std::vector<Note> song;
    for (const auto* part: {&partA, &partA, &partB})
        song.insert(song.end(), part->begin(), part->end());
    return song;
}

/// Bass: each bar alternates a root and its octave in eighth notes.
std::vector<Note> bassLine()
{
    const std::vector<std::pair<int, int>> partA = {
            {E2, E3}, {A2, A3}, {GS2, GS3}, {A2, A3}, {D2, D3}, {C2, C3}, {B1, B2}, {A2, A3}};
    const std::vector<std::pair<int, int>> partB = {
            {A2, A3}, {GS2, GS3}, {A2, A3}, {GS2, GS3}, {A2, A3}, {GS2, GS3}, {A2, A3}, {E2, E3}};

    std::vector<Note> song;
    for (const auto* part: {&partA, &partA, &partB})
        for (const auto& [low, high]: *part)
            for (int eighth = 0; eighth < 8; ++eighth)
                song.push_back({eighth % 2 == 0 ? low : high, .5f});
    return song;
}

SoundClip music()
{
    Tone lead;
    lead.waveform = Waveform::Square;
    lead.dutyCycle = 0.25f;
    lead.volume = 0.22f;
    lead.attack = 0.004f;
    lead.release = 0.06f;

    Tone bass;
    bass.waveform = Waveform::Triangle;
    bass.volume = 0.35f;
    bass.attack = 0.004f;
    bass.release = 0.04f;

    return mix({renderMelody(melody(), MUSIC_TEMPO, lead, MUSIC_SAMPLE_RATE),
                renderMelody(bassLine(), MUSIC_TEMPO, bass, MUSIC_SAMPLE_RATE)});
}

} // namespace

TetrisSounds createTetrisSounds()
{
    TetrisSounds sounds;

    sounds.move = share(render({blip(Waveform::Square, 520.f, 480.f, 0.025f, 0.12f, 0.25f)}));
    sounds.rotate = share(render({blip(Waveform::Triangle, 700.f, 1100.f, 0.05f, 0.35f)}));
    sounds.hold = share(render({blip(Waveform::Square, 440.f, 440.f, 0.04f, 0.15f, 0.25f),
                                blip(Waveform::Square, 660.f, 660.f, 0.06f, 0.15f, 0.25f)}));
    sounds.lock = share(render({blip(Waveform::Triangle, 240.f, 110.f, 0.07f, 0.45f)}));

    // A thud: a falling low tone with a burst of noise on top.
    sounds.hardDrop = share(mix({render({blip(Waveform::Triangle, 180.f, 50.f, 0.14f, 0.6f)}),
                                 render({blip(Waveform::Noise, 3000.f, 800.f, 0.08f, 0.18f)})}));

    // Bigger clears climb higher and longer.
    sounds.lineClear[0] = share(arpeggio({C5, E5, A5}, 0.045f, 0.18f, 0.5f));
    sounds.lineClear[1] = share(arpeggio({C5, E5, A5, C6}, 0.045f, 0.18f, 0.5f));
    sounds.lineClear[2] = share(arpeggio({A4, C5, E5, A5, C6, E6}, 0.04f, 0.18f, 0.5f));
    sounds.lineClear[3] = share(arpeggio({A4, C5, E5, A5, C6, E6, A6, E6, A6}, 0.04f, 0.2f, 0.25f));

    sounds.levelUp = share(arpeggio({E5, GS5, B5, E6}, 0.07f, 0.16f, 0.125f));
    sounds.pause = share(render({blip(Waveform::Square, 880.f, 880.f, 0.05f, 0.12f, 0.5f),
                                 blip(Waveform::Square, 660.f, 660.f, 0.07f, 0.12f, 0.5f)}));

    // A long, sad slide down.
    Tone fall = blip(Waveform::Square, 440.f, 55.f, 1.4f, 0.2f, 0.5f);
    fall.release = 0.6f;
    sounds.gameOver = share(render({fall}));

    sounds.music = share(music());
    return sounds;
}
