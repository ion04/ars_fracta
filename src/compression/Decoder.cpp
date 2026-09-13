#include "compression/Decoder.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ars::compression {

namespace {

/// Выборка пикселя домена из изображения v с применением изометрии.
float pixelAt(const std::vector<float>& v, int w, int dx, int dy, int scale,
              int r, int c, int iso, int n) {
    int sr = r, sc = c;
    switch (iso) {
        case 1: sr = c;       sc = n - 1 - r; break;
        case 2: sr = n - 1 - r; sc = n - 1 - c; break;
        case 3: sr = n - 1 - c; sc = r; break;
        case 4: sr = r;       sc = n - 1 - c; break;
        case 5: sr = n - 1 - r; sc = c; break;
        case 6: sr = c;       sc = r; break;
        case 7: sr = n - 1 - c; sc = n - 1 - r; break;
        default: break;
    }
    const int cy = dy + sr * scale;
    const int cx = dx + sc * scale;
    float acc = 0.0f;
    for (int sy = 0; sy < scale; ++sy) {
        for (int sx = 0; sx < scale; ++sx) {
            acc += v[static_cast<size_t>(cy + sy) * w + (cx + sx)];
        }
    }
    return acc / static_cast<float>(scale * scale);
}

} // namespace

utils::Image Decoder::decode(const Encoder::EncodedImage& enc, int iterations) {
    utils::Image img;
    if (enc.width <= 0 || enc.height <= 0 || enc.blockSize <= 0 || enc.codes.empty()) {
        return img;
    }

    img.width = enc.width;
    img.height = enc.height;
    img.channels = 1;
    img.data.assign(static_cast<size_t>(img.width) * img.height, 0);

    const int n = enc.blockSize;
    const int scale = enc.domainScale;
    const int w = img.width, h = img.height;

    std::vector<float> cur(static_cast<size_t>(w) * h, 0.0f);
    std::vector<float> next(static_cast<size_t>(w) * h, 0.0f);

    const int iters = std::max(1, iterations);
    for (int it = 0; it < iters; ++it) {
        for (const BlockCode& code : enc.codes) {
            const int rx = code.rangeX;
            const int ry = code.rangeY;
            for (int r = 0; r < n; ++r) {
                for (int c = 0; c < n; ++c) {
                    const float d = pixelAt(cur, w, code.domainX, code.domainY,
                                            scale, r, c, code.isometry, n);
                    float v = code.scale * d + code.offset;
                    v = std::max(0.0f, std::min(255.0f, v));
                    next[static_cast<size_t>(ry + r) * w + (rx + c)] = v;
                }
            }
        }
        cur.swap(next);
    }

    for (size_t i = 0; i < img.data.size(); ++i) {
        img.data[i] = static_cast<uint8_t>(std::round(cur[i]));
    }
    return img;
}

} // namespace ars::compression