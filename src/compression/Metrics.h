#pragma once

#include <cstddef>

#include "utils/ImageLoader.h"

namespace ars::compression {

struct MetricResult {
    double mse = 0.0;   // среднеквадратичная ошибка (в оттенках 0..255)
    double psnr = 0.0;  // пиковое отношение сигнал/шум, дБ
};

/// Расчёт MSE между двумя изображениями одинакового размера.
double computeMSE(const utils::Image& original, const utils::Image& decompressed);

/// PSNR по MSE (bpp = 255).
double computePSNR(double mse);

/// Коэффициент сжатия: originalBytes / encodedBytes (>1 — сжатие есть).
double compressionRatio(std::size_t originalBytes, std::size_t encodedBytes);

} // namespace ars::compression