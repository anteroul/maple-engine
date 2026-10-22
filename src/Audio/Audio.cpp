#include "Audio.h"
#include "../Log/msg.h"
#include <miniaudio.h>
#include <algorithm>
#include <cstdio>
#include <list>

namespace {

/// Most clips that may play at once. A burst of sound effects beyond this is
/// dropped rather than piling up voices.
constexpr size_t MAX_VOICES = 48;

std::unique_ptr<Audio> s_Instance;

} // namespace

/// One playing instance of a clip. miniaudio reads the samples straight out
/// of the clip, so the voice keeps the clip alive until it is done.
struct Voice {
    SoundClipPtr clip;
    ma_audio_buffer buffer{};
    ma_sound sound{};
    bool initialized = false;

    ~Voice()
    {
        if (!initialized)
            return;
        ma_sound_uninit(&sound);
        ma_audio_buffer_uninit(&buffer);
    }

    /// Prepare `clip` for playback on `engine`. The voice must not move
    /// afterwards: miniaudio keeps pointers into it.
    bool init(ma_engine& engine, const SoundClipPtr& source)
    {
        clip = source;

        ma_audio_buffer_config config = ma_audio_buffer_config_init(
                ma_format_f32, 1, clip->samples.size(), clip->samples.data(), nullptr);
        // Without a rate miniaudio assumes the engine's, and a clip made at
        // any other rate would play at the wrong speed.
        config.sampleRate = clip->sampleRate;

        if (ma_audio_buffer_init(&config, &buffer) != MA_SUCCESS)
            return false;

        // No spatialization: the engine is 2D, and sounds should come out of
        // both speakers evenly.
        if (ma_sound_init_from_data_source(&engine, &buffer, MA_SOUND_FLAG_NO_SPATIALIZATION,
                                           nullptr, &sound) != MA_SUCCESS)
        {
            ma_audio_buffer_uninit(&buffer);
            return false;
        }

        initialized = true;
        return true;
    }
};

struct Audio::Impl {
    ma_engine engine{};
    bool available = false;

    /// Sound effects still playing. Finished ones are reaped on the next play.
    std::list<std::unique_ptr<Voice>> voices;
    std::list<std::unique_ptr<ma_sound>> fileSounds;

    /// The music track: either a clip voice or a streamed file.
    std::unique_ptr<Voice> musicVoice;
    std::unique_ptr<ma_sound> musicStream;

    ma_sound* music()
    {
        if (musicVoice)
            return &musicVoice->sound;
        return musicStream.get();
    }

    void reapFinishedVoices()
    {
        voices.remove_if([](const std::unique_ptr<Voice>& voice) {
            return ma_sound_at_end(&voice->sound);
        });
        fileSounds.remove_if([](const std::unique_ptr<ma_sound>& sound) {
            if (!ma_sound_at_end(sound.get()))
                return false;
            ma_sound_uninit(sound.get());
            return true;
        });
    }

    size_t playingCount() const
    {
        return voices.size() + fileSounds.size();
    }

    void releaseMusic()
    {
        musicVoice.reset();

        if (musicStream)
        {
            ma_sound_uninit(musicStream.get());
            musicStream.reset();
        }
    }
};

Audio& Audio::getInstance()
{
    if (!s_Instance)
        s_Instance.reset(new Audio());
    return *s_Instance;
}

void Audio::shutdown()
{
    s_Instance.reset();
}

Audio::Audio() : m_Impl(std::make_unique<Impl>())
{
    const ma_result result = ma_engine_init(nullptr, &m_Impl->engine);

    if (result != MA_SUCCESS)
    {
        printf("%s \bWARNING: Audio disabled, no output device could be opened: %s\n",
               WARNING, ma_result_description(result));
        return;
    }

    m_Impl->available = true;
    printf("%s \bSUCCESS: Audio initialized at %u Hz.\n", SUCCESS,
           ma_engine_get_sample_rate(&m_Impl->engine));
}

Audio::~Audio()
{
    if (!m_Impl->available)
        return;

    // Sounds first: they belong to the engine and must not outlive it.
    m_Impl->voices.clear();
    for (auto& sound: m_Impl->fileSounds)
        ma_sound_uninit(sound.get());
    m_Impl->fileSounds.clear();
    m_Impl->releaseMusic();
    ma_engine_uninit(&m_Impl->engine);
}

bool Audio::isAvailable() const
{
    return m_Impl->available;
}

void Audio::play(const SoundClipPtr& clip, float volume, float pitch)
{
    if (!m_Impl->available || !clip || clip->samples.empty())
        return;

    m_Impl->reapFinishedVoices();

    if (m_Impl->playingCount() >= MAX_VOICES)
        return;

    auto voice = std::make_unique<Voice>();

    if (!voice->init(m_Impl->engine, clip))
        return;

    ma_sound_set_volume(&voice->sound, volume);
    ma_sound_set_pitch(&voice->sound, pitch);
    ma_sound_start(&voice->sound);
    m_Impl->voices.push_back(std::move(voice));
}

void Audio::play(const std::string& path, float volume)
{
    if (!m_Impl->available)
        return;

    m_Impl->reapFinishedVoices();

    if (m_Impl->playingCount() >= MAX_VOICES)
        return;

    auto sound = std::make_unique<ma_sound>();

    // Decoded in full, not streamed: sound effects are short, and miniaudio's
    // resource manager keeps the decoded file, so playing it again is cheap.
    if (ma_sound_init_from_file(&m_Impl->engine, path.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                nullptr, nullptr, sound.get()) != MA_SUCCESS)
    {
        printf("%s \bWARNING: Could not play sound: %s\n", WARNING, path.c_str());
        return;
    }

    ma_sound_set_volume(sound.get(), volume);
    ma_sound_start(sound.get());
    m_Impl->fileSounds.push_back(std::move(sound));
}

void Audio::playMusic(const SoundClipPtr& clip, float volume)
{
    if (!m_Impl->available || !clip || clip->samples.empty())
        return;

    m_Impl->releaseMusic();

    auto voice = std::make_unique<Voice>();

    if (!voice->init(m_Impl->engine, clip))
        return;

    ma_sound_set_looping(&voice->sound, MA_TRUE);
    ma_sound_set_volume(&voice->sound, volume);
    ma_sound_start(&voice->sound);
    m_Impl->musicVoice = std::move(voice);
}

void Audio::playMusic(const std::string& path, float volume)
{
    if (!m_Impl->available)
        return;

    m_Impl->releaseMusic();

    auto stream = std::make_unique<ma_sound>();

    // Streamed: music files are long, and decoding one up front would stall
    // the game and hold the whole track in memory.
    if (ma_sound_init_from_file(&m_Impl->engine, path.c_str(),
                                MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                nullptr, nullptr, stream.get()) != MA_SUCCESS)
    {
        printf("%s \bWARNING: Could not open music: %s\n", WARNING, path.c_str());
        return;
    }

    ma_sound_set_looping(stream.get(), MA_TRUE);
    ma_sound_set_volume(stream.get(), volume);
    ma_sound_start(stream.get());
    m_Impl->musicStream = std::move(stream);
}

void Audio::stopMusic()
{
    if (m_Impl->available)
        m_Impl->releaseMusic();
}

void Audio::setMusicPaused(bool paused)
{
    ma_sound* music = m_Impl->available ? m_Impl->music() : nullptr;

    if (!music)
        return;

    if (paused)
        ma_sound_stop(music);
    else
        ma_sound_start(music);
}

void Audio::setMusicPitch(float pitch)
{
    if (ma_sound* music = m_Impl->available ? m_Impl->music() : nullptr)
        ma_sound_set_pitch(music, pitch);
}

bool Audio::isMusicPlaying() const
{
    ma_sound* music = m_Impl->available ? m_Impl->music() : nullptr;
    return music && ma_sound_is_playing(music);
}

void Audio::setMasterVolume(float volume)
{
    if (m_Impl->available)
        ma_engine_set_volume(&m_Impl->engine, std::clamp(volume, 0.f, 1.f));
}
