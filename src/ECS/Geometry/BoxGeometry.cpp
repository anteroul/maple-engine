#include "BoxGeometry.h"

MeshData generateBoxGeometry(float width, float height, float depth)
{
    MeshData mesh;
    float hw = width  / 2.0f;
    float hh = height / 2.0f;
    float hd = depth  / 2.0f;

    mesh.vertices = {
        // +Z
        {{-hw,-hh, hd}, {0,0,1}, {0,0}}, {{ hw,-hh, hd}, {0,0,1}, {1,0}},
        {{ hw, hh, hd}, {0,0,1}, {1,1}}, {{-hw, hh, hd}, {0,0,1}, {0,1}},
        // -Z
        {{ hw,-hh,-hd}, {0,0,-1}, {0,0}}, {{-hw,-hh,-hd}, {0,0,-1}, {1,0}},
        {{-hw, hh,-hd}, {0,0,-1}, {1,1}}, {{ hw, hh,-hd}, {0,0,-1}, {0,1}},
        // +X
        {{ hw,-hh, hd}, {1,0,0}, {0,0}}, {{ hw,-hh,-hd}, {1,0,0}, {1,0}},
        {{ hw, hh,-hd}, {1,0,0}, {1,1}}, {{ hw, hh, hd}, {1,0,0}, {0,1}},
        // -X
        {{-hw,-hh,-hd}, {-1,0,0}, {0,0}}, {{-hw,-hh, hd}, {-1,0,0}, {1,0}},
        {{-hw, hh, hd}, {-1,0,0}, {1,1}}, {{-hw, hh,-hd}, {-1,0,0}, {0,1}},
        // +Y
        {{-hw, hh, hd}, {0,1,0}, {0,0}}, {{ hw, hh, hd}, {0,1,0}, {1,0}},
        {{ hw, hh,-hd}, {0,1,0}, {1,1}}, {{-hw, hh,-hd}, {0,1,0}, {0,1}},
        // -Y
        {{-hw,-hh,-hd}, {0,-1,0}, {0,0}}, {{ hw,-hh,-hd}, {0,-1,0}, {1,0}},
        {{ hw,-hh, hd}, {0,-1,0}, {1,1}}, {{-hw,-hh, hd}, {0,-1,0}, {0,1}},
    };

    mesh.indices.reserve(36);
    for (unsigned int face = 0; face < 6; ++face)
    {
        unsigned int base = face * 4;
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
        mesh.indices.push_back(base + 0);
    }

    return mesh;
}