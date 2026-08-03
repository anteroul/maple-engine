#include "MeshGPU.h"
#include <GL/glew.h>
#include <cstddef>

MeshGPUHandle uploadMesh(const MeshData& mesh)
{
    MeshGPUHandle handle;
    handle.indexCount = mesh.indices.size();

    glGenVertexArrays(1, &handle.vao);
    glGenBuffers(1, &handle.vbo);
    glGenBuffers(1, &handle.ebo);

    glBindVertexArray(handle.vao);

    glBindBuffer(GL_ARRAY_BUFFER, handle.vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int),
                 mesh.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
    return handle;
}

void drawMesh(const MeshGPUHandle& handle)
{
    glBindVertexArray(handle.vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)handle.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void destroyMesh(MeshGPUHandle& handle)
{
    if (handle.vao) glDeleteVertexArrays(1, &handle.vao);
    if (handle.vbo) glDeleteBuffers(1, &handle.vbo);
    if (handle.ebo) glDeleteBuffers(1, &handle.ebo);
    handle.vao = handle.vbo = handle.ebo = 0;
}