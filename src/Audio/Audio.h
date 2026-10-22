#ifndef MAPLEENGINE_AUDIO_H
#define MAPLEENGINE_AUDIO_H

#include <memory>
#include <string>
#include <vector>

/// Mono audio held in memory: a decoded sound, or one generated in code (see
/// Synth.h). One clip can be playing several times over at once.
struct SoundClip {
    std::vector<float> samples;
    unsigned int sampleRate = 44100;

    float duration() const { return sampleRate ? (float)samples.size() / (float)sampleRate : 0.f; }
};

using SoundClipPtr = std::shared_ptr<const SoundClip>;

/// The engine's audio output, built on miniaudio.
///
/// Sound effects are fire and forget: play() starts a clip or a file and the
/// engine cleans up once it ends. On top of that there is one music track,
/// which loops until it is stopped or replaced.
///
/// Audio is optional. If no output device can be opened (a headless machine,
/// a missing sound server) a warning is printed once and every call becomes a
/// no-op, so a game never has to check before playing a sound.
class Audio {
public:
    static Audio& getInstance();
    /// Close the output device. App calls this on exit; getInstance() after
    /// it opens the device again.
    static void shutdown();

    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool isAvailable() const;

    /// Play a clip once.
    /// \param volume Linear gain, 1 for the clip as it is.
    /// \param pitch Playback rate, 2 an octave up and twice as fast.
    void play(const SoundClipPtr& clip, float volume = 1.f, float pitch = 1.f);
    /// Play a sound file once. WAV, FLAC and MP3 are supported.
    void play(const std::string& path, float volume = 1.f);

    /// Loop `clip` as the music track, replacing whatever was playing.
    void playMusic(const SoundClipPtr& clip, float volume = 1.f);
    /// Stream a sound file as the music track.
    void playMusic(const std::string& path, float volume = 1.f);
    void stopMusic();
    void setMusicPaused(bool paused);
    void setMusicPitch(float pitch);
    bool isMusicPlaying() const;

    /// Gain applied to everything, sound effects and music alike.
    void setMasterVolume(float volume);

private:
    Audio();

    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif //MAPLEENGINE_AUDIO_H
