#include "SphereGeometry.h"
#include <cmath>

MeshData generateSphereGeometry(float radius, int sectors, int stacks)
{
    MeshData mesh;
    const float PI = 3.14159265359f;

    for (int i = 0; i <= stacks; ++i)
    {
        float stackAngle = PI / 2 - i * (PI / stacks);
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j)
        {
            float sectorAngle = j * (2 * PI / sectors);
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            Vertex v{};
            v.position[0] = x; v.position[1] = y; v.position[2] = z;
            v.normal[0] = x / radius; v.normal[1] = y / radius; v.normal[2] = z / radius;
            v.uv[0] = (float)j / sectors;
            v.uv[1] = (float)i / stacks;

            mesh.vertices.push_back(v);
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                mesh.indices.push_back(k1);
                mesh.indices.push_back(k2);
                mesh.indices.push_back(k1 + 1);
            }
            if (i != stacks - 1)
            {
                mesh.indices.push_back(k1 + 1);
                mesh.indices.push_back(k2);
                mesh.indices.push_back(k2 + 1);
            }
        }
    }

    return mesh;
}