#include "Synth.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace synth {

namespace {

constexpr float TWO_PI = 6.28318530718f;

/// Deterministic noise, so a clip sounds the same on every run.
class NoiseSource {
public:
    float next()
    {
        m_State = m_State * 1664525u + 1013904223u;
        return (float)(m_State >> 8) / (float)(1u << 24) * 2.f - 1.f;
    }

private:
    std::uint32_t m_State = 0x1234567u;
};

/// Value of a waveform at `phase`, which runs from 0 to 1 over one cycle.
float oscillator(const Tone& tone, float phase)
{
    switch (tone.waveform)
    {
        case Waveform::Square:   return phase < tone.dutyCycle ? 1.f : -1.f;
        case Waveform::Triangle: return 4.f * std::fabs(phase - 0.5f) - 1.f;
        case Waveform::Sine:     return std::sin(TWO_PI * phase);
        case Waveform::Sawtooth: return 2.f * phase - 1.f;
        case Waveform::Noise:    return 0.f;  // handled by the caller
    }
    return 0.f;
}

void renderTone(const Tone& tone, unsigned int sampleRate, std::vector<float>& out)
{
    const size_t count = (size_t)std::max(0.f, tone.duration * (float)sampleRate);
    const float ratio = tone.endFrequency / std::max(tone.startFrequency, 1.f);

    NoiseSource noise;
    float phase = 0.f;
    float noiseValue = noise.next();

    for (size_t i = 0; i < count; ++i)
    {
        const float t = (float)i / (float)sampleRate;
        const float progress = (float)i / (float)std::max<size_t>(count, 1);
        const float frequency = tone.startFrequency * std::pow(ratio, progress);

        float envelope = 1.f;
        if (tone.attack > 0.f && t < tone.attack)
            envelope = t / tone.attack;
        const float remaining = tone.duration - t;
        if (tone.release > 0.f && remaining < tone.release)
            envelope = std::min(envelope, remaining / tone.release);

        phase += frequency / (float)sampleRate;
        const bool wrapped = phase >= 1.f;
        phase -= std::floor(phase);

        float value;
        if (tone.waveform == Waveform::Noise)
        {
            if (wrapped)
                noiseValue = noise.next();
            value = noiseValue;
        }
        else
        {
            value = oscillator(tone, phase);
        }

        out.push_back(value * envelope * tone.volume);
    }
}

} // namespace

SoundClip render(const std::vector<Tone>& tones, unsigned int sampleRate)
{
    SoundClip clip;
    clip.sampleRate = sampleRate;

    for (const Tone& tone: tones)
        renderTone(tone, sampleRate, clip.samples);

    return clip;
}

float noteFrequency(int pitch)
{
    return 440.f * std::pow(2.f, (float)(pitch - 69) / 12.f);
}

SoundClip renderMelody(const std::vector<Note>& notes, float beatsPerMinute, const Tone& instrument,
                       unsigned int sampleRate)
{
    const float secondsPerBeat = 60.f / beatsPerMinute;
    std::vector<Tone> tones;
    tones.reserve(notes.size());

    for (const Note& note: notes)
    {
        Tone tone = instrument;
        tone.duration = note.beats * secondsPerBeat;

        if (note.pitch == REST)
            tone.volume = 0.f;
        else
            tone.startFrequency = tone.endFrequency = noteFrequency(note.pitch);

        tones.push_back(tone);
    }

    return render(tones, sampleRate);
}

SoundClip mix(const std::vector<SoundClip>& clips)
{
    SoundClip result;

    for (const SoundClip& clip: clips)
    {
        result.sampleRate = clip.sampleRate;

        if (clip.samples.size() > result.samples.size())
            result.samples.resize(clip.samples.size(), 0.f);

        for (size_t i = 0; i < clip.samples.size(); ++i)
            result.samples[i] += clip.samples[i];
    }

    return result;
}

} // namespace synth
