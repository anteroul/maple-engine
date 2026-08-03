#ifndef MAPLEENGINE_MESHGPU_H
#define MAPLEENGINE_MESHGPU_H

#include "../Geometry/MeshData.h"

struct MeshGPUHandle {
    unsigned int vao = 0, vbo = 0, ebo = 0;
    size_t indexCount = 0;
};

MeshGPUHandle uploadMesh(const MeshData& mesh);
void drawMesh(const MeshGPUHandle& handle);
void destroyMesh(MeshGPUHandle& handle);

#endif