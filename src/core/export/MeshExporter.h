#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/math/Vector3.h"

namespace ars::core::exporter {

/** @brief Triangle mesh represented as independent triangles. */
struct SurfaceMesh {
    std::vector<math::Vector3> vertices;

    /** @return true when the mesh contains no triangles. */
    bool empty() const noexcept { return vertices.empty(); }
    /** @return the number of triangles in the mesh. */
    std::size_t triangleCount() const noexcept { return vertices.size() / 3; }
};

/** @brief Writes CPU-generated fractal meshes to interchange formats. */
class MeshExporter {
public:
    /** @brief Write a triangle mesh in Wavefront OBJ format. */
    static bool writeObj(const std::string& path, const SurfaceMesh& mesh);

    /** @brief Write a triangle mesh in binary STL format. */
    static bool writeBinaryStl(const std::string& path, const SurfaceMesh& mesh);
};

} // namespace ars::core::exporter
