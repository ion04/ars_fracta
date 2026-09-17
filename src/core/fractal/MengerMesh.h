#pragma once

#include <functional>

#include "core/export/MeshExporter.h"

namespace ars::core::fractal {

/** @brief Builds polygonal representations of the Menger sponge. */
class MengerMesh {
public:
    /**
     * @brief Generate the external voxel faces of a Menger sponge.
     *
     * `iterations` is limited to [0, 4] to bound CPU time and memory use.
     * @param iterations Recursion depth.
     * @param isCancelled Optional callback, polled per voxel row.
     * @return Empty mesh if cancellation was requested; otherwise external faces.
     */
    static exporter::SurfaceMesh build(int iterations,
                                      const std::function<bool()>& isCancelled = {});
};

} // namespace ars::core::fractal
