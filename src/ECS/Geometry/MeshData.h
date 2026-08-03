#ifndef MAPLEENGINE_MESHDATA_H
#define MAPLEENGINE_MESHDATA_H

#include <vector>

struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};

#endif