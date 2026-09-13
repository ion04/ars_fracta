#include "compression/Metrics.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ars::compression {

double computeMSE(const utils::Image& orig, const utils::Image& dec) {
    if (orig.width != dec.width || orig.height != dec.height || orig.data.empty()) {
        return std::numeric_limits<double>::max();
    }

    const std::size_t npix = static_cast<std::size_t>(orig.width) * orig.height;
    const int c1 = orig.channels > 0 ? orig.channels : 1;
    const int c2 = dec.channels > 0 ? dec.channels : 1;

    if (c1 == 1 && c2 == 1) {
        double acc = 0.0;
        for (std::size_t i = 0; i < npix; ++i) {
            const double e = static_cast<double>(orig.data[i]) - dec.data[i];
            acc += e * e;
        }
        return acc / static_cast<double>(npix);
    }

    const int c = std::min(c1, c2);
    double acc = 0.0;
    for (std::size_t p = 0; p < npix; ++p) {
        double e2 = 0.0;
        for (int k = 0; k < c; ++k) {
            const double e = static_cast<double>(orig.data[p * c1 + k]) -
                             dec.data[p * c2 + k];
            e2 += e * e;
        }
        acc += e2 / c;
    }
    return acc / static_cast<double>(npix);
}

double computePSNR(double mse) {
    if (mse <= 0.0) return 99.9;
    return 10.0 * std::log10(255.0 * 255.0 / mse);
}

double compressionRatio(std::size_t originalBytes, std::size_t encodedBytes) {
    if (encodedBytes == 0) return std::numeric_limits<double>::max();
    return static_cast<double>(originalBytes) / static_cast<double>(encodedBytes);
}

} // namespace ars::compression