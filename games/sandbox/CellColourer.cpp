#include "CellColourer.h"
#include "Rendering/ShaderProgram.h"
#include "Shaders/cells.comp.h"
#include <algorithm>
#include <cctype>
#include <string>

using namespace sandbox;

namespace {

/// The cells.comp shader is dispatched in 16 x 16 groups.
constexpr int GROUP_SIZE = 16;

/// "#define MAT_SAND 2u" and so on, for every material, so the shader
/// agrees with the Material enum.
std::string materialDefines()
{
    std::string defines;
    for (int i = 0; i < (int)Material::Count; ++i)
    {
        std::string name = info((Material)i).name;
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return (char)std::toupper(c); });
        defines += "#define MAT_" + name + " " + std::to_string(i) + "u\n";
    }
    return defines;
}

} // namespace

CellColourer::~CellColourer()
{
    if (renderingReleased())
        return;
    glDeleteProgram(m_Program);
    glDeleteTextures(1, &m_Cells);
}

bool CellColourer::ready(int width, int height)
{
    if (m_Failed)
        return false;

    if (!m_Program)
    {
        if (!computeSupported()
            || !(m_Program = linkCompute(CELLS_COMP, "cell colour", materialDefines().c_str())))
        {
            m_Failed = true;
            return false;
        }
        m_FrameUniform = glGetUniformLocation(m_Program, "u_Frame");
        m_CellsUniform = glGetUniformLocation(m_Program, "u_Cells");
    }

    if (!m_Cells || width != m_Width || height != m_Height)
    {
        if (!m_Cells)
            glGenTextures(1, &m_Cells);
        m_Width = width;
        m_Height = height;
        glBindTexture(GL_TEXTURE_2D, m_Cells);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, width, height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);
        // Integer textures cannot be filtered.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_Packed.assign((std::size_t)width * height * 4, 0);
    }
    return true;
}

bool CellColourer::colour(const Simulation& simulation, std::uint32_t frame, PixelCanvas& canvas)
{
    const int width = simulation.width(), height = simulation.height();
    if (!ready(width, height))
        return false;

    std::uint8_t* out = m_Packed.data();
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x, out += 4)
        {
            const Cell& cell = simulation.at(x, y);
            out[0] = (std::uint8_t)cell.material;
            out[1] = cell.shade;
            out[2] = cell.burning ? 1 : 0;
            // The colours stop changing with life well below 255.
            out[3] = (std::uint8_t)std::clamp<int>(cell.life, 0, 255);
        }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_Cells);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, m_Packed.data());

    glUseProgram(m_Program);
    glUniform1i(m_CellsUniform, 0);
    glUniform1ui(m_FrameUniform, frame);
    canvas.bindImage(0);
    glDispatchCompute((width + GROUP_SIZE - 1) / GROUP_SIZE, (height + GROUP_SIZE - 1) / GROUP_SIZE, 1);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // The canvas is sampled as a texture when it is drawn.
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    return true;
}
