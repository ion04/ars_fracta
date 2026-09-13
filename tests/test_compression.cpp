#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>

#include "compression/Decoder.h"
#include "compression/Encoder.h"
#include "compression/Metrics.h"
#include "core/fractal/Mandelbulb3D.h"
#include "core/fractal/Mandelbrot2D.h"
#include "core/math/Complex.h"
#include "core/math/Vector3.h"
#include "utils/ImageLoader.h"

using namespace ars;

namespace {
int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cout << "[FAIL] " << what << '\n';
        ++failures;
    } else {
        std::cout << "[ OK ] " << what << '\n';
    }
}
} // namespace

/// Построить синтетическое тестовое изображение — множество Мандельброта (CPU).
static utils::Image makeMandelbrotImage(int size) {
    utils::Image img;
    img.width = size;
    img.height = size;
    img.channels = 1;
    img.data.resize(static_cast<std::size_t>(size) * size);

    const int maxIter = 128;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const double u = (static_cast<double>(x) + 0.5) / size * 3.0 - 2.25;
            const double v = (static_cast<double>(y) + 0.5) / size * 2.4 - 1.2;
            const core::math::Complex c(u, v);
            const int it = core::fractal::Mandelbrot2D::escapeTime(c, maxIter);
            img.data[static_cast<std::size_t>(y) * size + x] =
                static_cast<std::uint8_t>((it * 255) / maxIter);
        }
    }
    return img;
}

int main() {
    std::cout << "=== ars_fracta: test_compression ===\n\n";

    // --- Complex ---
    {
        const core::math::Complex a(3.0, 4.0);
        check(std::fabs(a.magnitudeSquared() - 25.0) < 1e-9,
              "Complex::magnitudeSquared (3+4i) == 25");

        const core::math::Complex z = a.squared();
        check(std::fabs(z.re - (-7.0)) < 1e-9 && std::fabs(z.im - 24.0) < 1e-9,
              "Complex::squared (3+4i)^2 == -7+24i");

        const core::math::Complex w(2.0, 0.0);
        const core::math::Complex wp = w.pow(3.0);
        check(std::fabs(wp.re - 8.0) < 1e-9 && std::fabs(wp.im) < 1e-9,
              "Complex::pow 2^3 == 8");
    }

    // --- Vector3 ---
    {
        const core::math::Vector3 v(1.0, 2.0, 3.0);
        check(std::fabs(v.length() - std::sqrt(14.0)) < 1e-9,
              "Vector3::length == sqrt(14)");
        check(v.dot(v) == 14.0, "Vector3::dot(v,v) == 14");
        const core::math::Vector3 ex(1, 0, 0);
        const core::math::Vector3 gy(0, 1, 0);
        const core::math::Vector3 cross = ex.cross(gy);
        check(cross.x == 0.0 && cross.y == 0.0 && cross.z == 1.0,
              "Vector3::cross(x,y) == z");
    }

    // --- Mandelbrot CPU: точка внутри и точка снаружи ---
    {
        const core::math::Complex inside(0.0, 0.0);  // z=0 — внутри
        const core::math::Complex outside(2.0, 2.0); // убегает сразу
        check(core::fractal::Mandelbrot2D::escapeTime(inside, 100) == 100,
              "escapeTime(0,0) == maxIter (точка внутри)");
        const int e = core::fractal::Mandelbrot2D::escapeTime(outside, 100);
        check(e >= 0 && e < 100, "escapeTime(2,2) быстро убегает (t < 100)");
    }

    // --- Mandelbulb DE ---
    {
        const double dZero =
            core::fractal::Mandelbulb3D::distanceEstimate(core::math::Vector3(0, 0, 0), 8.0, 32);
        check(std::isfinite(dZero), "Mandelbulb DE(NULL) finite");
        const double dFar =
            core::fractal::Mandelbulb3D::distanceEstimate(core::math::Vector3(4, 0, 0), 8.0, 32);
        check(dFar > dZero + 0.5, "Mandelbulb DE: дальняя точка дальше от поверхности");
    }

    // --- Фрактальное сжатие: цикл encode -> decode ---
    {
        const int size = 128;
        const utils::Image src = makeMandelbrotImage(size);

        compression::Encoder enc;
        const auto encoded = enc.encode(src, 8, 2);
        check(encoded.width == size && !encoded.codes.empty(),
              "Encoder::encode вернул валидный код");

        const auto decoded = compression::Decoder::decode(encoded, 8);
        check(decoded.width == size && decoded.height == size,
              "Decoder::decode вернул изображение нужного размера");

        const double mse = compression::computeMSE(src, decoded);
        const double psnr = compression::computePSNR(mse);
        const double ratio =
            compression::compressionRatio(src.data.size(), encoded.byteSize());

        std::cout << "        MSE = " << std::setprecision(2) << mse
                  << ", PSNR = " << psnr << " dB"
                  << ", ratio = " << ratio << ":1\n";
        check(psnr > 18.0, "PSNR > 18 дБ (восстановление приемлемого качества)");
        check(ratio > 1.0, "Есть компрессия: ratio > 1");
    }

    // --- Сериализация кода в файл и обратно ---
    {
        const utils::Image src = makeMandelbrotImage(64);
        compression::Encoder enc;
        const auto encoded = enc.encode(src, 8, 2);
        check(encoded.codes.size() == 64, "64 range-блока для 64x64 / 8x8");

        const std::string path = "test_compression_output.frax";
        check(encoded.writeFile(path), "writeFile()");

        compression::Encoder::EncodedImage loaded;
        check(compression::Encoder::EncodedImage::readFile(path, loaded), "readFile()");
        check(loaded.codes.size() == encoded.codes.size(),
              "Round-trip: количество кодов совпадает");

        if (loaded.codes.size() == encoded.codes.size()) {
            bool same = true;
            for (std::size_t i = 0; i < encoded.codes.size(); ++i) {
                const auto& a = encoded.codes[i];
                const auto& b = loaded.codes[i];
                if (a.rangeX != b.rangeX || a.domainX != b.domainX ||
                    a.isometry != b.isometry || a.scale != b.scale ||
                    a.offset != b.offset) {
                    same = false;
                    break;
                }
            }
            check(same, "Round-trip: все поля кодов совпадают");
        }
        std::remove(path.c_str());
    }

    std::cout << "\n=== Итог: " << failures << " ошибок ===\n";
    return failures == 0 ? 0 : 1;
}