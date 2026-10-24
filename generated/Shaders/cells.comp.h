// Generated from cells.comp by cmake/EmbedShader.cmake. Edit the shader, not this.
#pragma once

inline constexpr const char CELLS_COMP[] = R"maple_shader(#version 430
// Colours the falling sand grid: one thread per cell, from the cells the CPU
// packed into a texture straight into the canvas the sandbox draws.
//
// A port of cellColour() in SandboxController.cpp, which is still used where
// compute shaders are not. Change one, change the other. The MAT_* numbers
// come from the C++ Material enum, so those cannot drift apart.

layout(local_size_x = 16, local_size_y = 16) in;

/// r: material, g: shade, b: burning, a: life, clamped to 255.
uniform usampler2D u_Cells;
layout(rgba16f, binding = 0) writeonly uniform image2D u_Canvas;
uniform uint u_Frame;

/// Cheap per-pixel noise, for flicker that differs from cell to cell.
float noise(uint x, uint y, uint t)
{
    uint h = x * 374761393u + y * 668265263u + t * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return float((h ^ (h >> 16)) & 0xFFFFu) / 65535.0;
}

void main()
{
    const ivec2 size = textureSize(u_Cells, 0);
    const ivec2 at = ivec2(gl_GlobalInvocationID.xy);
    if (at.x >= size.x || at.y >= size.y)
        return;

    const uvec4 cell = texelFetch(u_Cells, at, 0);
    const uint material = cell.r;
    const float shade = float(cell.g) / 255.0;
    const bool burning = cell.b != 0u;
    const float life = float(cell.a);
    const float flicker = noise(uint(at.x), uint(at.y), u_Frame / 3u);

    vec3 rgb = vec3(0.0);
    float alpha = 1.0;

    if (material == MAT_EMPTY)
    {
        imageStore(u_Canvas, at, vec4(0.0));
        return;
    }
    else if (material == MAT_STONE)
        rgb = vec3(0.33, 0.33, 0.36) * (0.8 + 0.35 * shade);
    else if (material == MAT_SAND)
        rgb = vec3(0.86, 0.71, 0.42) * (0.82 + 0.25 * shade);
    else if (material == MAT_WATER)
    {
        rgb = vec3(0.13, 0.33, 0.78) * (0.9 + 0.15 * shade);
        alpha = 0.85;
    }
    else if (material == MAT_WOOD)
        rgb = vec3(0.40, 0.24, 0.12) * (0.75 + 0.4 * shade);
    else if (material == MAT_OIL)
    {
        rgb = vec3(0.22, 0.15, 0.07) * (0.8 + 0.35 * shade);
        alpha = 0.95;
    }
    else if (material == MAT_GUNPOWDER)
        rgb = vec3(0.22, 0.22, 0.25) * (0.6 + 0.7 * shade);
    else if (material == MAT_LAVA)
    {
        const float wave = 0.5 + 0.5 * sin(float(u_Frame) * 0.05 + shade * 6.283);
        rgb = mix(vec3(2.4, 0.5, 0.06), vec3(3.6, 1.4, 0.25), wave);
    }
    else if (material == MAT_FIRE)
    {
        const float heat = clamp(life / 30.0, 0.0, 1.0);
        rgb = mix(vec3(2.0, 0.3, 0.04), vec3(4.5, 2.6, 0.8), heat) * (0.7 + 0.6 * flicker);
    }
    else if (material == MAT_SMOKE)
    {
        rgb = vec3(0.34, 0.33, 0.34) * (0.85 + 0.3 * shade);
        alpha = 0.25 + 0.5 * clamp(life / 200.0, 0.0, 1.0);
    }
    else if (material == MAT_STEAM)
    {
        rgb = vec3(0.75, 0.8, 0.86);
        alpha = 0.12 + 0.35 * clamp(life / 200.0, 0.0, 1.0);
    }
    else if (material == MAT_ASH)
        rgb = vec3(0.46, 0.44, 0.42) * (0.8 + 0.4 * shade);

    if (burning)
        rgb = mix(rgb, vec3(3.2, 1.1, 0.2) * (0.6 + 0.8 * flicker), 0.65);

    imageStore(u_Canvas, at, vec4(rgb, alpha));
}
)maple_shader";
