#include "DoomAudio.h"
#include "Audio/Synth.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

using namespace synth;
using doom::Sound;

namespace {

constexpr float TWO_PI = 2.f * std::numbers::pi_v<float>;
constexpr unsigned int RATE = DEFAULT_SAMPLE_RATE;
/// Music is long, and loses nothing audible at half the rate.
constexpr unsigned int MUSIC_RATE = 22050;

SoundClip silence(float seconds, unsigned int rate = RATE)
{
    SoundClip clip;
    clip.sampleRate = rate;
    clip.samples.assign((std::size_t)(seconds * (float)rate), 0.f);
    return clip;
}

/// Add `source` into `target` starting `at` seconds in, growing the target
/// to fit.
void mixInto(SoundClip& target, const SoundClip& source, float at = 0.f, float gain = 1.f)
{
    const std::size_t start = (std::size_t)(at * (float)target.sampleRate);
    if (target.samples.size() < start + source.samples.size())
        target.samples.resize(start + source.samples.size(), 0.f);
    for (std::size_t i = 0; i < source.samples.size(); ++i)
        target.samples[start + i] += source.samples[i] * gain;
}

/// Noise through a low pass filter whose cutoff slides from one frequency
/// to another, dying away exponentially: gunshots, blasts, doors.
SoundClip noiseBurst(float duration, float cutoffStart, float cutoffEnd, float decay, float volume,
                     unsigned int seed, unsigned int rate = RATE)
{
    SoundClip clip = silence(duration, rate);
    std::mt19937 random(seed);
    std::uniform_real_distribution<float> white(-1.f, 1.f);
    float filtered = 0.f;

    for (std::size_t i = 0; i < clip.samples.size(); ++i)
    {
        const float t = (float)i / (float)rate;
        const float progress = t / duration;
        const float cutoff = cutoffStart * std::pow(cutoffEnd / cutoffStart, progress);
        const float alpha = 1.f - std::exp(-TWO_PI * cutoff / (float)rate);
        filtered += alpha * (white(random) - filtered);

        const float attack = std::min(1.f, t / 0.002f);
        const float tail = std::min(1.f, (duration - t) / 0.01f);
        clip.samples[i] = filtered * std::exp(-t * decay) * attack * tail * volume;
    }
    return clip;
}

/// A sine dropping in pitch: the low thump under gunshots and blasts.
SoundClip thump(float duration, float startHz, float endHz, float volume)
{
    SoundClip clip = silence(duration);
    float phase = 0.f;
    for (std::size_t i = 0; i < clip.samples.size(); ++i)
    {
        const float t = (float)i / (float)RATE;
        const float frequency = startHz * std::pow(endHz / startHz, t / duration);
        phase += TWO_PI * frequency / (float)RATE;
        clip.samples[i] = std::sin(phase) * std::exp(-t * 6.f / duration) * volume;
    }
    return clip;
}

/// A throaty voice for monsters and the player: a sawtooth sliding in
/// pitch through `points`, roughened with noise and a wobble, muffled by a
/// low pass filter.
SoundClip voice(std::vector<float> points, float duration, float roughness, float cutoff, float volume,
                unsigned int seed)
{
    SoundClip clip = silence(duration);
    std::mt19937 random(seed);
    std::uniform_real_distribution<float> white(-1.f, 1.f);
    float phase = 0.f, filtered = 0.f, smoothedNoise = 0.f;
    const float alpha = 1.f - std::exp(-TWO_PI * cutoff / (float)RATE);

    for (std::size_t i = 0; i < clip.samples.size(); ++i)
    {
        const float t = (float)i / (float)RATE;
        const float progress = t / duration * (float)(points.size() - 1);
        const std::size_t segment = std::min(points.size() - 2, (std::size_t)progress);
        const float within = progress - (float)segment;
        float frequency = points[segment] * std::pow(points[segment + 1] / points[segment], within);
        frequency *= 1.f + 0.03f * std::sin(TWO_PI * 7.f * t);

        phase += frequency / (float)RATE;
        phase -= std::floor(phase);
        smoothedNoise += 0.3f * (white(random) - smoothedNoise);
        const float source = (2.f * phase - 1.f) * (1.f - roughness) + smoothedNoise * roughness * 2.f;
        filtered += alpha * (source - filtered);

        // A growl: the loudness flutters thirty times a second.
        const float growl = 1.f - roughness * 0.4f * (0.5f + 0.5f * std::sin(TWO_PI * 30.f * t));
        const float attack = std::min(1.f, t / 0.015f);
        const float release = std::min(1.f, (duration - t) / (duration * 0.45f));
        clip.samples[i] = filtered * growl * attack * release * volume;
    }
    return clip;
}

SoundClip gunshot(float duration, float brightness, float decay, float volume, unsigned int seed)
{
    SoundClip clip = noiseBurst(duration, brightness, 400.f, decay, volume, seed);
    mixInto(clip, thump(duration * 0.5f, 150.f, 45.f, volume * 0.7f));
    return clip;
}

SoundClip explosion(float duration, float volume, unsigned int seed)
{
    SoundClip clip = noiseBurst(duration, 2600.f, 120.f, 3.f / duration * 1.2f, volume, seed);
    mixInto(clip, thump(duration * 0.6f, 70.f, 28.f, volume * 0.9f));
    mixInto(clip, noiseBurst(duration * 0.5f, 900.f, 200.f, 8.f, volume * 0.6f, seed + 1), 0.08f);
    return clip;
}

SoundClip click(float volume, unsigned int seed)
{
    return noiseBurst(0.035f, 7000.f, 3000.f, 120.f, volume, seed);
}

/// A machine clanking along: doors.
SoundClip doorSound(bool closing, unsigned int seed)
{
    SoundClip clip = noiseBurst(0.55f, 380.f, 220.f, 2.f, 0.9f, seed);
    SoundClip motor = render({{Waveform::Square, 62.f, closing ? 55.f : 70.f, 0.5f, 0.12f, 0.02f, 0.1f, 0.3f}});
    mixInto(clip, motor);
    mixInto(clip, click(0.5f, seed + 1));
    mixInto(clip, thump(0.18f, 110.f, 50.f, 0.6f), closing ? 0.48f : 0.42f);
    return clip;
}

Tone blip(Waveform waveform, float startHz, float endHz, float duration, float volume, float duty = 0.5f)
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

SoundClip pickupSound(bool weapon)
{
    if (!weapon)
        return render({blip(Waveform::Square, 880.f, 1320.f, 0.07f, 0.35f, 0.25f)});

    std::vector<Tone> tones;
    for (float frequency: {523.25f, 659.25f, 783.99f, 1046.5f})
        tones.push_back(blip(Waveform::Square, frequency, frequency, 0.06f, 0.35f, 0.25f));
    tones.back().duration = 0.18f;
    tones.back().release = 0.14f;
    return render(tones);
}

// ----------------------------------------------------------------- music --

enum : int {
    B1 = 35, C2 = 36, D2 = 38, DS2 = 39, E2 = 40, FS2 = 42, G2 = 43, A2 = 45, B2 = 47,
    C4 = 60, D4 = 62, DS4 = 63, E4 = 64, FS4 = 66, G4 = 67, A4 = 69, B4 = 71,
    C5 = 72, D5 = 74, DS5 = 75, E5 = 76, FS5 = 78, G5 = 79
};

constexpr float TEMPO = 138.f;

/// Galloping eighths in E minor, a bar at a time.
std::vector<Note> bassLine()
{
    const std::vector<std::vector<int>> bars = {
            {E2, E2, G2, E2, A2, E2, G2, FS2},
            {E2, E2, G2, E2, B2, A2, G2, FS2},
            {C2, C2, E2, C2, D2, D2, FS2, D2},
            {B1, B1, DS2, B1, FS2, E2, DS2, B1},
    };

    std::vector<Note> notes;
    for (int pass = 0; pass < 2; ++pass)
        for (const auto& bar: bars)
            for (int pitch: bar)
                notes.push_back({pitch, 0.5f});
    return notes;
}

std::vector<Note> leadLine()
{
    std::vector<Note> notes = {
            {E4, 2.f}, {G4, 1.f}, {FS4, 1.f},
            {E4, 1.f}, {B4, 1.f}, {A4, 1.f}, {G4, 1.f},
            {C5, 2.f}, {B4, 1.f}, {A4, 1.f},
            {B4, 2.f}, {A4, 1.f}, {FS4, 1.f},
            // Second time round, higher and busier.
            {E5, 1.5f}, {D5, 0.5f}, {E5, 1.f}, {G5, 1.f},
            {FS5, 1.f}, {E5, 1.f}, {D5, 1.f}, {B4, 1.f},
            {C5, 1.5f}, {D5, 0.5f}, {E5, 1.f}, {C5, 1.f},
            {B4, 2.f}, {DS5, 1.f}, {FS5, 1.f},
    };
    return notes;
}

SoundClip drums(float beats)
{
    const float beat = 60.f / TEMPO;
    SoundClip clip = silence(beats * beat, MUSIC_RATE);

    SoundClip kick;
    kick.sampleRate = MUSIC_RATE;
    {
        float phase = 0.f;
        const int length = (int)(0.18f * MUSIC_RATE);
        for (int i = 0; i < length; ++i)
        {
            const float t = (float)i / (float)MUSIC_RATE;
            phase += TWO_PI * (45.f + 90.f * std::exp(-t * 30.f)) / (float)MUSIC_RATE;
            kick.samples.push_back(std::sin(phase) * std::exp(-t * 18.f) * 0.7f);
        }
    }
    const SoundClip snare = noiseBurst(0.16f, 5000.f, 1500.f, 22.f, 0.35f, 7, MUSIC_RATE);
    const SoundClip hat = noiseBurst(0.04f, 9000.f, 7000.f, 80.f, 0.12f, 9, MUSIC_RATE);

    for (int eighth = 0; eighth < (int)(beats * 2.f); ++eighth)
    {
        const float at = (float)eighth * beat / 2.f;
        const int inBar = eighth % 8;
        if (inBar == 0 || inBar == 3 || inBar == 4)
            mixInto(clip, kick, at);
        if (inBar == 2 || inBar == 6)
            mixInto(clip, snare, at);
        mixInto(clip, hat, at);
    }
    return clip;
}

SoundClip music()
{
    Tone bass;
    bass.waveform = Waveform::Square;
    bass.volume = 0.32f;
    bass.dutyCycle = 0.25f;
    bass.release = 0.06f;

    Tone lead;
    lead.waveform = Waveform::Sawtooth;
    lead.volume = 0.16f;
    lead.attack = 0.01f;
    lead.release = 0.12f;

    SoundClip song = mix({renderMelody(bassLine(), TEMPO, bass, MUSIC_RATE),
                          renderMelody(leadLine(), TEMPO, lead, MUSIC_RATE),
                          drums(32.f)});
    for (float& sample: song.samples)
        sample = std::clamp(sample, -1.f, 1.f);
    return song;
}

} // namespace

const SoundClipPtr& DoomSounds::clipFor(const doom::SoundEvent& event) const
{
    switch (event.sound)
    {
        case Sound::MonsterAlert: return voices[(int)event.source][0];
        case Sound::MonsterPain: return voices[(int)event.source][1];
        case Sound::MonsterDeath: return voices[(int)event.source][2];
        default: return effects[(int)event.sound];
    }
}

DoomSounds createDoomSounds()
{
    DoomSounds sounds;
    auto set = [&](Sound sound, SoundClip clip) { sounds.effects[(int)sound] = share(std::move(clip)); };

    {
        SoundClip punch = thump(0.1f, 130.f, 60.f, 0.9f);
        mixInto(punch, noiseBurst(0.06f, 1500.f, 600.f, 40.f, 0.6f, 1));
        set(Sound::Punch, punch);
    }
    set(Sound::Pistol, gunshot(0.3f, 6000.f, 16.f, 0.8f, 2));
    {
        SoundClip shotgun = gunshot(0.55f, 4500.f, 8.f, 1.f, 3);
        mixInto(shotgun, thump(0.25f, 110.f, 35.f, 0.6f));
        // Working the pump: click, clack.
        mixInto(shotgun, click(0.45f, 4), 0.42f);
        mixInto(shotgun, noiseBurst(0.06f, 3000.f, 1200.f, 50.f, 0.4f, 5), 0.6f);
        set(Sound::Shotgun, shotgun);
    }
    set(Sound::Chaingun, gunshot(0.18f, 6500.f, 22.f, 0.7f, 6));
    {
        SoundClip launch = noiseBurst(0.6f, 1600.f, 300.f, 4.f, 0.7f, 7);
        mixInto(launch, thump(0.2f, 120.f, 50.f, 0.6f));
        set(Sound::RocketLaunch, launch);
    }
    set(Sound::NoAmmo, click(0.5f, 8));
    set(Sound::Explosion, explosion(1.3f, 1.f, 9));
    set(Sound::FireballLaunch, noiseBurst(0.45f, 600.f, 3000.f, 4.f, 0.55f, 10));
    set(Sound::FireballBurst, explosion(0.5f, 0.6f, 11));
    {
        SoundClip plasma = render({blip(Waveform::Square, 1400.f, 300.f, 0.25f, 0.25f, 0.3f)});
        mixInto(plasma, noiseBurst(0.3f, 2000.f, 4000.f, 6.f, 0.3f, 12));
        set(Sound::PlasmaLaunch, plasma);
    }
    {
        SoundClip burst = render({blip(Waveform::Sine, 900.f, 90.f, 0.35f, 0.5f)});
        mixInto(burst, noiseBurst(0.4f, 2500.f, 300.f, 8.f, 0.5f, 13));
        set(Sound::PlasmaBurst, burst);
    }
    set(Sound::TrooperShot, gunshot(0.3f, 4500.f, 14.f, 0.7f, 14));
    {
        // A swipe through the air, then the hit.
        SoundClip claw = noiseBurst(0.18f, 2500.f, 7000.f, 10.f, 0.5f, 15);
        mixInto(claw, thump(0.1f, 140.f, 70.f, 0.5f), 0.12f);
        set(Sound::Claw, claw);
    }
    {
        SoundClip bite = voice({95.f, 70.f}, 0.3f, 0.6f, 900.f, 0.8f, 16);
        mixInto(bite, thump(0.08f, 160.f, 80.f, 0.7f), 0.05f);
        set(Sound::Bite, bite);
    }
    set(Sound::PlayerPain, voice({230.f, 160.f}, 0.22f, 0.25f, 1600.f, 0.8f, 17));
    set(Sound::PlayerDeath, voice({300.f, 260.f, 90.f}, 0.95f, 0.3f, 1500.f, 0.85f, 18));
    set(Sound::PlayerGrunt, voice({170.f, 140.f}, 0.14f, 0.3f, 1200.f, 0.6f, 19));
    set(Sound::DoorOpen, doorSound(false, 20));
    set(Sound::DoorClose, doorSound(true, 21));
    set(Sound::ItemPickup, pickupSound(false));
    set(Sound::WeaponPickup, pickupSound(true));
    {
        SoundClip exit = click(0.6f, 22);
        std::vector<Tone> chord;
        for (float frequency: {329.63f, 392.f, 493.88f, 659.25f})
            chord.push_back(blip(Waveform::Square, frequency, frequency, 0.1f, 0.3f, 0.25f));
        mixInto(exit, render(chord), 0.05f);
        set(Sound::ExitSwitch, exit);
    }

    // Monster voices: alert, pain and death, each in its own register.
    auto voices = [&](doom::ThingType type, SoundClip alert, SoundClip pain, SoundClip death) {
        sounds.voices[(int)type] = {share(std::move(alert)), share(std::move(pain)), share(std::move(death))};
    };
    voices(doom::ThingType::Trooper,
           voice({150.f, 190.f, 120.f}, 0.4f, 0.45f, 1300.f, 0.8f, 30),
           voice({300.f, 200.f}, 0.16f, 0.35f, 1800.f, 0.7f, 31),
           voice({260.f, 200.f, 70.f}, 0.7f, 0.45f, 1400.f, 0.8f, 32));
    voices(doom::ThingType::Imp,
           voice({420.f, 900.f, 300.f}, 0.55f, 0.55f, 2600.f, 0.65f, 33),
           voice({650.f, 420.f}, 0.18f, 0.5f, 2600.f, 0.6f, 34),
           voice({700.f, 500.f, 90.f}, 0.85f, 0.55f, 2400.f, 0.7f, 35));
    voices(doom::ThingType::Demon,
           voice({110.f, 130.f, 70.f}, 0.7f, 0.7f, 900.f, 0.95f, 36),
           voice({220.f, 130.f}, 0.22f, 0.6f, 1100.f, 0.8f, 37),
           voice({180.f, 140.f, 40.f}, 1.f, 0.7f, 900.f, 0.95f, 38));
    voices(doom::ThingType::Knight,
           voice({80.f, 95.f, 55.f}, 1.f, 0.65f, 700.f, 1.f, 39),
           voice({160.f, 100.f}, 0.25f, 0.6f, 900.f, 0.85f, 40),
           voice({140.f, 110.f, 30.f}, 1.5f, 0.65f, 700.f, 1.f, 41));

    sounds.music = share(music());
    return sounds;
}
