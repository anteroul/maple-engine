#ifndef MAPLESANDBOX_CELLCOLOURER_H
#define MAPLESANDBOX_CELLCOLOURER_H

#include "Simulation.h"
#include "Rendering/PixelCanvas.h"
#include <GL/glew.h>
#include <cstdint>
#include <vector>

/// Colours the grid on the GPU: the cells go up as 4 bytes each, and a
/// compute shader (shaders/cells.comp) turns them into the canvas's pixels.
/// That saves working out 57,600 colours a frame on the CPU, and uploading
/// them as 16 bytes each.
class CellColourer {
public:
    CellColourer() = default;
    ~CellColourer();
    CellColourer(const CellColourer&) = delete;
    CellColourer& operator=(const CellColourer&) = delete;

    /// Fill `canvas` with the colours of the simulation's cells, which must
    /// be the same size. Returns false, doing nothing, if compute shaders are
    /// not available: the caller then colours the cells itself.
    bool colour(const sandbox::Simulation& simulation, std::uint32_t frame, PixelCanvas& canvas);

private:
    /// Builds the shader and texture on first use. False if they cannot be.
    bool ready(int width, int height);

    GLuint m_Program = 0;
    GLint m_FrameUniform = -1;
    GLint m_CellsUniform = -1;
    GLuint m_Cells = 0;
    int m_Width = 0;
    int m_Height = 0;
    bool m_Failed = false;
    std::vector<std::uint8_t> m_Packed;
};

#endif //MAPLESANDBOX_CELLCOLOURER_H
