#include "PostProcess.h"
#include "PostProcessShaders.h"
#include "ShaderProgram.h"
#include "../ECS/Entity.h"
#include "../ECS/Components/Glow.h"
#include "../Log/msg.h"
#include <algorithm>
#include <cstdio>

namespace {

GLint uniform(GLuint program, const char* name)
{
    return glGetUniformLocation(program, name);
}

} // namespace

void PostProcess::setSettings(const BloomSettings& settings)
{
    m_Settings = settings;
    m_Settings.levels = std::clamp(m_Settings.levels, 1, 16);
    // The chain depends on the level count, so rebuild it on the next frame.
    m_Width = m_Height = 0;
}

bool PostProcess::beginScene(int width, int height)
{
    if (!m_Settings.enabled || m_Unsupported || width <= 0 || height <= 0)
        return false;

    if (!m_HasPrograms)
    {
        // Framebuffer objects and floating point textures are both OpenGL 3.0.
        if (!GLEW_VERSION_3_0)
        {
            disable("it needs OpenGL 3.0");
            return false;
        }
        if (!createPrograms())
        {
            disable("its shaders did not build");
            return false;
        }
    }

    if (!resize(width, height))
    {
        disable("its framebuffers could not be created");
        return false;
    }

    bindTarget(m_Scene);
    // Let glColor3f pass colours above 1 through to the floating point
    // buffer, so that a game can make something bloom just by drawing it
    // brighter than white.
    glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
    return true;
}

void PostProcess::beginGlow()
{
    // The viewport is not framebuffer state, so the scene's letterboxed one
    // carries over.
    glBindFramebuffer(GL_FRAMEBUFFER, m_Glow.framebuffer);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void PostProcess::drawGlow(const Entity& entity, const Glow& glow)
{
    // Bound per entity: a renderer with a Shader component of its own binds
    // that, then unbinds it.
    glUseProgram(m_GlowProgram);

    const auto& colour = glow.getColour();
    if (colour)
        glUniform3f(uniform(m_GlowProgram, "u_Colour"), colour->r, colour->g, colour->b);
    glUniform1f(uniform(m_GlowProgram, "u_UseColour"), colour ? 1.f : 0.f);
    glUniform1f(uniform(m_GlowProgram, "u_Intensity"), glow.getIntensity());

    entity.render();

    glUseProgram(0);
}

void PostProcess::finish()
{
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Bright part of the scene, plus the glow, into the first level.
    const float knee = std::max(m_Settings.threshold * m_Settings.softKnee, 1e-5f);
    glUseProgram(m_PrefilterProgram);
    glUniform1i(uniform(m_PrefilterProgram, "u_Scene"), 0);
    glUniform1i(uniform(m_PrefilterProgram, "u_Glow"), 1);
    glUniform4f(uniform(m_PrefilterProgram, "u_Threshold"),
                m_Settings.threshold, m_Settings.threshold - knee, 2.f * knee, 0.25f / knee);
    bindTexture(0, m_Scene);
    bindTexture(1, m_Glow);
    bindTarget(m_Levels[0]);
    drawFullscreenQuad();

    // Halve it down the chain.
    glUseProgram(m_DownsampleProgram);
    glUniform1i(uniform(m_DownsampleProgram, "u_Source"), 0);
    for (int i = 1; i < m_LevelCount; ++i)
    {
        const Target& source = m_Levels[i - 1];
        glUniform2f(uniform(m_DownsampleProgram, "u_TexelSize"),
                    1.f / (float)source.width, 1.f / (float)source.height);
        bindTexture(0, source);
        bindTarget(m_Levels[i]);
        drawFullscreenQuad();
    }

    // And blur it back up, each level added onto the one above.
    glUseProgram(m_UpsampleProgram);
    glUniform1i(uniform(m_UpsampleProgram, "u_Source"), 0);
    glUniform1f(uniform(m_UpsampleProgram, "u_Radius"), m_Settings.radius);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = m_LevelCount - 2; i >= 0; --i)
    {
        const Target& source = m_Levels[i + 1];
        glUniform2f(uniform(m_UpsampleProgram, "u_TexelSize"),
                    1.f / (float)source.width, 1.f / (float)source.height);
        bindTexture(0, source);
        bindTarget(m_Levels[i]);
        drawFullscreenQuad();
    }
    glDisable(GL_BLEND);

    // The first level now holds every level's light, so divide by the count
    // to keep the intensity independent of how many there are.
    const Target& bloom = m_Levels[0];
    glUseProgram(m_CompositeProgram);
    glUniform1i(uniform(m_CompositeProgram, "u_Scene"), 0);
    glUniform1i(uniform(m_CompositeProgram, "u_Bloom"), 1);
    glUniform2f(uniform(m_CompositeProgram, "u_BloomTexelSize"),
                m_Settings.radius / (float)bloom.width, m_Settings.radius / (float)bloom.height);
    glUniform1f(uniform(m_CompositeProgram, "u_Intensity"),
                m_Settings.intensity / (float)m_LevelCount);
    bindTexture(0, m_Scene);
    bindTexture(1, bloom);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_Width, m_Height);
    drawFullscreenQuad();

    glUseProgram(0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}

void PostProcess::release()
{
    releaseTargets();

    for (GLuint* program : {&m_GlowProgram, &m_PrefilterProgram, &m_DownsampleProgram,
                            &m_UpsampleProgram, &m_CompositeProgram})
    {
        if (*program)
            glDeleteProgram(*program);
        *program = 0;
    }
    m_HasPrograms = false;
}

bool PostProcess::createPrograms()
{
    using namespace shaders;

    m_GlowProgram = linkProgram(GLOW_VERTEX, GLOW_FRAGMENT, "glow");
    m_PrefilterProgram = linkProgram(FULLSCREEN_VERTEX, PREFILTER_FRAGMENT, "bloom prefilter");
    m_DownsampleProgram = linkProgram(FULLSCREEN_VERTEX, DOWNSAMPLE_FRAGMENT, "bloom downsample");
    m_UpsampleProgram = linkProgram(FULLSCREEN_VERTEX, UPSAMPLE_FRAGMENT, "bloom upsample");
    m_CompositeProgram = linkProgram(FULLSCREEN_VERTEX, COMPOSITE_FRAGMENT, "bloom composite");

    m_HasPrograms = m_GlowProgram && m_PrefilterProgram && m_DownsampleProgram
                    && m_UpsampleProgram && m_CompositeProgram;
    return m_HasPrograms;
}

bool PostProcess::resize(int width, int height)
{
    if (width == m_Width && height == m_Height)
        return true;

    releaseTargets();

    if (!createTarget(m_Scene, width, height, true) || !createTarget(m_Glow, width, height, false))
        return false;

    int levelWidth = width / 2;
    int levelHeight = height / 2;
    while ((int)m_Levels.size() < m_Settings.levels && levelWidth >= 2 && levelHeight >= 2)
    {
        if (!createTarget(m_Levels.emplace_back(), levelWidth, levelHeight, false))
            return false;
        levelWidth /= 2;
        levelHeight /= 2;
    }
    // A window under 4 pixels a side still gets one level.
    if (m_Levels.empty() && !createTarget(m_Levels.emplace_back(), std::max(width / 2, 1),
                                          std::max(height / 2, 1), false))
        return false;

    m_LevelCount = (int)m_Levels.size();
    m_Width = width;
    m_Height = height;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void PostProcess::releaseTargets()
{
    destroyTarget(m_Scene);
    destroyTarget(m_Glow);
    for (Target& level : m_Levels)
        destroyTarget(level);
    m_Levels.clear();
    m_LevelCount = 0;
    m_Width = m_Height = 0;
}

void PostProcess::disable(const char* reason)
{
    printf("%s \bWARNING: Bloom is off, because %s.\n", WARNING, reason);
    release();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_Unsupported = true;
}

bool PostProcess::createTarget(Target& target, int width, int height, bool withDepth)
{
    target.width = width;
    target.height = height;

    glGenTextures(1, &target.texture);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);

    // Only the scene needs depth: World clears it, and a game may test it.
    if (withDepth)
    {
        glGenRenderbuffers(1, &target.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, target.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.depth);
    }

    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void PostProcess::destroyTarget(Target& target)
{
    if (target.framebuffer)
        glDeleteFramebuffers(1, &target.framebuffer);
    if (target.texture)
        glDeleteTextures(1, &target.texture);
    if (target.depth)
        glDeleteRenderbuffers(1, &target.depth);
    target = Target{};
}

void PostProcess::bindTarget(const Target& target)
{
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, target.width, target.height);
}

void PostProcess::bindTexture(int unit, const Target& target)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, target.texture);
}

void PostProcess::drawFullscreenQuad()
{
    glBegin(GL_QUADS);
    glVertex2f(-1.f, -1.f);
    glVertex2f(1.f, -1.f);
    glVertex2f(1.f, 1.f);
    glVertex2f(-1.f, 1.f);
    glEnd();
}
