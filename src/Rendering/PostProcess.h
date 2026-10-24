#ifndef MAPLEENGINE_POSTPROCESS_H
#define MAPLEENGINE_POSTPROCESS_H

#include <GL/glew.h>
#include <vector>

class Entity;
class Glow;

/// How the scene blooms. Pass to World::setBloom().
struct BloomSettings {
    /// Off by default, and then the scene draws straight to the window with
    /// no offscreen buffers at all.
    bool enabled = false;
    /// Brightness, the largest of red, green and blue, from which the scene
    /// starts to bloom. The scene is drawn in floating point, so colours can
    /// go above 1, and at the default only those do. Glow components bloom
    /// whatever this is.
    float threshold = 1.f;
    /// Fraction of the threshold over which bloom fades in below it, rather
    /// than switching on at the threshold: 0 is a hard cut.
    float softKnee = 0.5f;
    /// How strongly the blurred light is added back over the scene.
    float intensity = 1.f;
    /// Spread of each blur step, in pixels of that step. Above about 2 the
    /// halo starts to look blocky.
    float radius = 1.f;
    /// Number of times the light is halved and blurred. More levels give a
    /// wider halo, at about a third of the cost of the level before.
    int levels = 6;
};

/// Bloom and glow, done after the scene is drawn.
///
/// With bloom on, the scene is drawn into a floating point buffer, and any
/// glowing entity is drawn a second time, in its glow colour, into a glow
/// buffer. The bright part of the scene plus the glow buffer is then halved
/// level by level and blurred back up, and the result is added over the
/// scene as it is copied to the window.
class PostProcess {
public:
    PostProcess() = default;
    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    void setSettings(const BloomSettings& settings);
    const BloomSettings& getSettings() const { return m_Settings; }

    /// Send drawing to the offscreen scene buffer, sized to the window's
    /// framebuffer. Returns false when bloom is off or the GPU cannot do it,
    /// in which case draw straight to the window as usual.
    bool beginScene(int width, int height);
    /// Send drawing to the glow buffer instead, cleared, keeping the viewport
    /// and projection the scene was drawn with. Only after beginScene()
    /// returned true.
    void beginGlow();
    /// Draw one glowing entity into the glow buffer.
    void drawGlow(const Entity& entity, const Glow& glow);
    /// Blur, and draw the scene with its bloom to the window.
    void finish();

    /// Free the GL objects. Must run while the context still exists.
    void release();

private:
    struct Target {
        GLuint framebuffer = 0;
        GLuint texture = 0;
        GLuint depth = 0;
        int width = 0;
        int height = 0;
    };

    BloomSettings m_Settings;
    /// Set once bloom has proved impossible here, so it is not retried, and
    /// complained about, every frame.
    bool m_Unsupported = false;
    bool m_HasPrograms = false;

    GLuint m_GlowProgram = 0;
    GLuint m_PrefilterProgram = 0;
    GLuint m_DownsampleProgram = 0;
    GLuint m_UpsampleProgram = 0;
    GLuint m_CompositeProgram = 0;

    Target m_Scene;
    Target m_Glow;
    /// The bloom chain, each half the size of the one before, starting at
    /// half the window.
    std::vector<Target> m_Levels;
    int m_Width = 0;
    int m_Height = 0;
    int m_LevelCount = 0;

    bool createPrograms();
    bool resize(int width, int height);
    void releaseTargets();
    void disable(const char* reason);

    static bool createTarget(Target& target, int width, int height, bool withDepth);
    static void destroyTarget(Target& target);
    static void bindTarget(const Target& target);
    static void bindTexture(int unit, const Target& target);
    static void drawFullscreenQuad();
};

#endif //MAPLEENGINE_POSTPROCESS_H
