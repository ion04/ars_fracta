#include "compression/Encoder.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

namespace ars::compression {

namespace {

constexpr int kIsometryCount = 8;

/// Применение изометрии 0..7 к координатам (r, c) блока размера n.
int isometryRow(int iso, int r, int c, int n) {
    switch (iso) {
        case 1: return c;            // поворот на 90°
        case 2: return n - 1 - r;    // поворот на 180°
        case 3: return n - 1 - c;    // поворот на 270°
        case 4: return r;            // отражение по горизонтали
        case 5: return n - 1 - r;    // отражение по вертикали
        case 6: return c;            // отражение по главной диагонали
        case 7: return n - 1 - c;    // отражение по побочной диагонали
        default: return r;
    }
}

int isometryCol(int iso, int r, int c, int n) {
    switch (iso) {
        case 1: return n - 1 - r;
        case 2: return n - 1 - c;
        case 3: return r;
        case 4: return n - 1 - c;
        case 5: return c;
        case 6: return r;
        case 7: return n - 1 - r;
        default: return c;
    }
}

/// Сборка блока b[n*n] из домена d[n*n] с применением изометрии.
void gatherBlock(const float* d, int n, int iso, float* out) {
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            const int sr = isometryRow(iso, r, c, n);
            const int sc = isometryCol(iso, r, c, n);
            out[r * n + c] = d[sr * n + sc];
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
//  EncodedImage
// ---------------------------------------------------------------------------

size_t Encoder::EncodedImage::byteSize() const {
    return 4 + 4 * 4 + codes.size() * sizeof(BlockCode);
}

bool Encoder::EncodedImage::writeFile(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const char magic[4] = {'A', 'R', 'S', 'F'};
    f.write(magic, 4);
    const int header[4] = {width, height, blockSize, domainScale};
    f.write(reinterpret_cast<const char*>(header), sizeof(header));
    for (const auto& c : codes) {
        f.write(reinterpret_cast<const char*>(&c), sizeof(c));
    }
    return static_cast<bool>(f);
}

bool Encoder::EncodedImage::readFile(const std::string& path, EncodedImage& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char magic[4];
    f.read(magic, 4);
    if (std::memcmp(magic, "ARSF", 4) != 0) return false;

    int header[4];
    f.read(reinterpret_cast<char*>(header), sizeof(header));
    out.width = header[0];
    out.height = header[1];
    out.blockSize = header[2];
    out.domainScale = header[3];
    if (out.width <= 0 || out.height <= 0 || out.blockSize <= 0 || out.domainScale <= 0) {
        return false;
    }

    const size_t blocksW = static_cast<size_t>(out.width) / out.blockSize;
    const size_t blocksH = static_cast<size_t>(out.height) / out.blockSize;
    const size_t expected = blocksW * blocksH;

    out.codes.clear();
    out.codes.reserve(expected);
    BlockCode code;
    while (f.read(reinterpret_cast<char*>(&code), sizeof(code))) {
        out.codes.push_back(code);
    }
    return out.codes.size() == expected;
}

// ---------------------------------------------------------------------------
//  Encoder
// ---------------------------------------------------------------------------

Encoder::EncodedImage Encoder::encode(const utils::Image& gray, int blockSize,
                                      int domainScale) {
    EncodedImage result;
    if (gray.channels != 1 || gray.data.empty() || blockSize <= 0 ||
        blockSize > 8 || domainScale <= 1) {
        return result;
    }

    const int w = gray.width, h = gray.height;
    const int n = blockSize;
    const int domainSize = n * domainScale;
    if (w < domainSize || h < domainSize) return result;

    result.width = w;
    result.height = h;
    result.blockSize = n;
    result.domainScale = domainScale;

    const int nn = n * n;
    const int stride = domainSize;

    // ---- пул доменов с предвычисленными данными ----
    struct Domain {
        float data[64];
        float mean = 0.0f;
        float var = 0.0f;
        int ox = 0;
        int oy = 0;
    };
    std::vector<Domain> pool;
    for (int dy = 0; dy + domainSize <= h; dy += stride) {
        for (int dx = 0; dx + domainSize <= w; dx += stride) {
            Domain d;
            d.ox = dx;
            d.oy = dy;
            double sum = 0.0;
            for (int r = 0; r < n; ++r) {
                for (int c = 0; c < n; ++c) {
                    uint32_t acc = 0;
                    const int cx = dx + c * domainScale;
                    const int cy = dy + r * domainScale;
                    for (int sy = 0; sy < domainScale; ++sy) {
                        for (int sx = 0; sx < domainScale; ++sx) {
                            acc += gray.data[static_cast<size_t>(cy + sy) * w + (cx + sx)];
                        }
                    }
                    const float v = static_cast<float>(acc) /
                                    static_cast<float>(domainScale * domainScale);
                    d.data[r * n + c] = v;
                    sum += v;
                }
            }
            d.mean = static_cast<float>(sum / nn);
            double v2 = 0.0;
            for (int i = 0; i < nn; ++i) {
                const double t = d.data[i] - d.mean;
                v2 += t * t;
            }
            d.var = static_cast<float>(v2 / nn);
            pool.push_back(d);
        }
    }

    if (pool.empty()) return result;

    // ---- подбор лучшего домена для каждого range-блока ----
    float range[64];
    float tmp[64];
    std::vector<BlockCode>& codes = result.codes;

    for (int ry = 0; ry + n <= h; ry += n) {
        for (int rx = 0; rx + n <= w; rx += n) {
            double rSum = 0.0;
            for (int r = 0; r < n; ++r) {
                for (int c = 0; c < n; ++c) {
                    const float v =
                        static_cast<float>(gray.data[static_cast<size_t>(ry + r) * w + (rx + c)]);
                    range[r * n + c] = v;
                    rSum += v;
                }
            }
            const float meanR = static_cast<float>(rSum / nn);

            BlockCode best;
            double bestSse = std::numeric_limits<double>::max();

            for (const Domain& dom : pool) {
                for (int iso = 0; iso < kIsometryCount; ++iso) {
                    gatherBlock(dom.data, n, iso, tmp);

                    double mD = 0.0;
                    for (int i = 0; i < nn; ++i) mD += tmp[i];
                    mD /= nn;

                    double varD = 0.0, cov = 0.0;
                    for (int i = 0; i < nn; ++i) {
                        const double td = tmp[i] - mD;
                        varD += td * td;
                        cov += td * (range[i] - meanR);
                    }

                    // минимизация SSE -> оценка Жакина
                    double s = (varD > 1e-9) ? cov / varD : 0.0;
                    s = std::max(-1.0, std::min(1.0, s));
                    const double o = meanR - s * mD;

                    double sse = 0.0;
                    for (int i = 0; i < nn; ++i) {
                        const double e = range[i] - (s * tmp[i] + o);
                        sse += e * e;
                    }
                    if (sse < bestSse) {
                        bestSse = sse;
                        best.rangeX = static_cast<uint16_t>(rx);
                        best.rangeY = static_cast<uint16_t>(ry);
                        best.domainX = static_cast<uint16_t>(dom.ox);
                        best.domainY = static_cast<uint16_t>(dom.oy);
                        best.isometry = static_cast<uint8_t>(iso);
                        best.scale = static_cast<float>(s);
                        best.offset = static_cast<float>(o);
                    }
                }
            }
            codes.push_back(best);
        }
    }
    return result;
}

} // namespace ars::compression