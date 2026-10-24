#include "PixelCanvas.h"
#include "ShaderProgram.h"
#include <algorithm>

namespace {

// A shader rather than fixed-function texturing, which may clamp the
// texture's colours to 1 on the way through.
constexpr const char* CANVAS_VERTEX = R"(
#version 120
varying vec2 v_UV;

void main()
{
    v_UV = gl_MultiTexCoord0.xy;
    gl_Position = ftransform();
}
)";

constexpr const char* CANVAS_FRAGMENT = R"(
#version 120
varying vec2 v_UV;
uniform sampler2D u_Pixels;

void main()
{
    gl_FragColor = texture2D(u_Pixels, v_UV);
}
)";

/// Kept for the life of the program, like the particle shader.
GLuint canvasProgram()
{
    static GLuint program = linkProgram(CANVAS_VERTEX, CANVAS_FRAGMENT, "pixel canvas");
    return program;
}

} // namespace

PixelCanvas::PixelCanvas(int width, int height)
    : m_Width(width), m_Height(height), m_Pixels((std::size_t)width * height, glm::vec4{0.f})
{}

PixelCanvas::~PixelCanvas()
{
    if (m_Texture && !renderingReleased())
        glDeleteTextures(1, &m_Texture);
}

void PixelCanvas::fill(const glm::vec4& colour)
{
    std::fill(m_Pixels.begin(), m_Pixels.end(), colour);
    m_Dirty = true;
}

void PixelCanvas::createTexture() const
{
    if (m_Texture)
        return;

    glGenTextures(1, &m_Texture);
    glBindTexture(GL_TEXTURE_2D, m_Texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, nullptr);
    // Nearest, so every pixel stays a crisp square however large it is drawn.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_Dirty = true;
}

void PixelCanvas::bindImage(GLuint unit)
{
    createTexture();
    glBindImageTexture(unit, m_Texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    // The GPU's pixels win over the stale ones on the CPU.
    m_Dirty = false;
}

void PixelCanvas::draw(glm::vec2 bottomLeft, glm::vec2 size) const
{
    const GLuint program = canvasProgram();
    if (program == 0)
        return;

    createTexture();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_Texture);
    if (m_Dirty)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_FLOAT, m_Pixels.data());
        m_Dirty = false;
    }

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "u_Pixels"), 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const glm::vec2 topRight = bottomLeft + size;
    glBegin(GL_QUADS);
    glTexCoord2f(0.f, 0.f); glVertex2f(bottomLeft.x, bottomLeft.y);
    glTexCoord2f(1.f, 0.f); glVertex2f(topRight.x, bottomLeft.y);
    glTexCoord2f(1.f, 1.f); glVertex2f(topRight.x, topRight.y);
    glTexCoord2f(0.f, 1.f); glVertex2f(bottomLeft.x, topRight.y);
    glEnd();

    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPopAttrib();
}
