#include "core/fractal/MengerMesh.h"

#include <algorithm>
#include <array>

namespace ars::core::fractal {

namespace {

constexpr int kMaxIterations = 4;

bool isSolidVoxel(int x, int y, int z, int size) {
    while (size > 1) {
        size /= 3;
        const int centeredAxes = (x / size % 3 == 1) + (y / size % 3 == 1) +
                                 (z / size % 3 == 1);
        if (centeredAxes >= 2) return false;
    }
    return true;
}

void appendQuad(exporter::SurfaceMesh& mesh, const math::Vector3& a,
                const math::Vector3& b, const math::Vector3& c,
                const math::Vector3& d) {
    mesh.vertices.insert(mesh.vertices.end(), {a, b, c, a, c, d});
}

} // namespace

exporter::SurfaceMesh MengerMesh::build(int iterations,
                                      const std::function<bool()>& isCancelled) {
    exporter::SurfaceMesh mesh;
    iterations = std::clamp(iterations, 0, kMaxIterations);

    int size = 1;
    for (int index = 0; index < iterations; ++index) size *= 3;
    const double step = 2.0 / static_cast<double>(size);

    constexpr std::array<std::array<int, 3>, 6> directions = {{{-1, 0, 0}, {1, 0, 0},
                                                                  {0, -1, 0}, {0, 1, 0},
                                                                  {0, 0, -1}, {0, 0, 1}}};
    for (int z = 0; z < size; ++z) {
        for (int y = 0; y < size; ++y) {
            if (isCancelled && isCancelled()) return {};
            for (int x = 0; x < size; ++x) {
                if (!isSolidVoxel(x, y, z, size)) continue;

                const double x0 = -1.0 + x * step;
                const double y0 = -1.0 + y * step;
                const double z0 = -1.0 + z * step;
                const double x1 = x0 + step;
                const double y1 = y0 + step;
                const double z1 = z0 + step;
                const std::array<math::Vector3, 8> corners = {{{x0, y0, z0}, {x1, y0, z0},
                                                                   {x1, y1, z0}, {x0, y1, z0},
                                                                   {x0, y0, z1}, {x1, y0, z1},
                                                                   {x1, y1, z1}, {x0, y1, z1}}};
                const std::array<std::array<int, 4>, 6> faces = {{{0, 4, 7, 3}, {1, 2, 6, 5},
                                                                      {0, 1, 5, 4}, {3, 7, 6, 2},
                                                                      {0, 3, 2, 1}, {4, 5, 6, 7}}};
                for (std::size_t face = 0; face < faces.size(); ++face) {
                    const auto& direction = directions[face];
                    const int neighborX = x + direction[0];
                    const int neighborY = y + direction[1];
                    const int neighborZ = z + direction[2];
                    const bool hasSolidNeighbor = neighborX >= 0 && neighborX < size &&
                                                  neighborY >= 0 && neighborY < size &&
                                                  neighborZ >= 0 && neighborZ < size &&
                                                  isSolidVoxel(neighborX, neighborY, neighborZ, size);
                    if (!hasSolidNeighbor) {
                        const auto& quad = faces[face];
                        appendQuad(mesh, corners[quad[0]], corners[quad[1]],
                                   corners[quad[2]], corners[quad[3]]);
                    }
                }
            }
        }
    }
    return mesh;
}

} // namespace ars::core::fractal
