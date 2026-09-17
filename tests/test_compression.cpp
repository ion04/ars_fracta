#include <cmath>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include "compression/Decoder.h"
#include "compression/Encoder.h"
#include "compression/Metrics.h"
#include "core/fractal/Mandelbulb3D.h"
#include "core/fractal/Mandelbrot2D.h"
#include "core/fractal/MengerSponge.h"
#include "core/fractal/Julia3D.h"
#include "core/fractal/Fractal.h"
#include "core/math/Complex.h"
#include "core/math/Vector3.h"
#include "core/async/JobSystem.h"
#include "core/export/MeshExporter.h"
#include "core/fractal/MengerMesh.h"
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

    // --- Smoke generation for all six supported fractal types ---
    {
        using core::fractal::FractalParams;
        using core::fractal::FractalType;
        using core::math::Vector3;

        const FractalType types[] = {
            FractalType::Mandelbrot2D, FractalType::Mandelbulb3D,
            FractalType::MengerSponge, FractalType::Julia3D,
            FractalType::Terrain3D, FractalType::Coast3D};
        bool namesPresent = true;
        for (const auto type : types) namesPresent = namesPresent && std::string(core::fractal::toString(type)) != "Unknown";
        check(namesPresent, "Все 6 типов фракталов зарегистрированы");

        core::fractal::Mandelbrot2D mandelbrot;
        FractalParams mandelbrotParams;
        mandelbrotParams.type = FractalType::Coast3D;
        mandelbrotParams.iterations = 4;
        mandelbrot.apply(mandelbrotParams);
        check(mandelbrot.type() == FractalType::Mandelbrot2D &&
                  mandelbrot.params().iterations >= 16 &&
                  std::isfinite(core::fractal::Mandelbrot2D::smoothEscapeTime({0.25, 0.1}, 32)),
              "Mandelbrot2D генерирует конечное значение без ошибки");

        core::fractal::Mandelbulb3D mandelbulb;
        FractalParams mandelbulbParams;
        mandelbulbParams.iterations = 4;
        mandelbulbParams.power = 1.0f;
        mandelbulb.apply(mandelbulbParams);
        check(mandelbulb.type() == FractalType::Mandelbulb3D &&
                  std::isfinite(core::fractal::Mandelbulb3D::distanceEstimate(
                      Vector3(0.2, 0.1, 0.3), mandelbulb.params().power,
                      mandelbulb.params().iterations)),
              "Mandelbulb3D генерирует конечную оценку расстояния");

        core::fractal::MengerSponge menger;
        FractalParams mengerParams;
        mengerParams.iterations = 0;
        menger.apply(mengerParams);
        check(menger.type() == FractalType::MengerSponge &&
                  std::isfinite(core::fractal::MengerSponge::distanceEstimate(
                      Vector3(0.1, 0.2, 0.3), menger.params().iterations)),
              "MengerSponge генерирует конечную оценку расстояния");

        core::fractal::Julia3D julia;
        FractalParams juliaParams;
        juliaParams.power = 1.0f;
        juliaParams.iterations = 4;
        julia.apply(juliaParams);
        check(julia.type() == FractalType::Julia3D &&
                  std::isfinite(core::fractal::Julia3D::distanceEstimate(
                      Vector3(0.2, 0.1, 0.3), julia.constant(), julia.params().power,
                      julia.params().iterations)),
              "Julia3D генерирует конечную оценку расстояния");

        std::ifstream shader("assets/shaders/fractal.frag");
        const std::string shaderText((std::istreambuf_iterator<char>(shader)),
                                     std::istreambuf_iterator<char>());
        check(!shaderText.empty() && shaderText.find("uFractalType == 4") != std::string::npos &&
                  shaderText.find("uFractalType == 5") != std::string::npos &&
                  shaderText.find("renderTerrain") != std::string::npos &&
                  shaderText.find("renderCoast") != std::string::npos,
              "Terrain3D и Coast3D имеют shader-путь генерации");
    }

    // --- Асинхронные CPU-задачи: UI может опрашивать ready без ожидания ---
    {
        using core::async::JobSystem;
        std::atomic_bool started{false};
        auto job = JobSystem::submit([&started](const auto& token) {
            started.store(true);
            while (!token.isCancelled()) std::this_thread::yield();
        });
        while (!started.load()) std::this_thread::yield();
        check(!job.ready(), "Job не блокирует UI до завершения worker");
        job.cancel();
        job.wait();
        check(job.cancelled(), "Job принимает отмену");
    }

    // --- Исключение worker передаётся вызывающему потоку через wait() ---
    {
        auto job = core::async::JobSystem::submit([](const auto&) {
            throw std::runtime_error("test worker failure");
        });
        bool exceptionDelivered = false;
        try {
            job.wait();
        } catch (const std::runtime_error&) {
            exceptionDelivered = true;
        }
        check(job.ready(), "Job становится ready после ошибки worker");
        check(exceptionDelivered, "Job::wait передаёт исключение worker");
    }

    // Bounded regression: a missing cancellation request must fail, not hang CTest.
    {
        bool cancelledOnDestruction = false;
        {
            auto job = core::async::JobSystem::submit([&](const auto& token) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                while (!token.isCancelled() && std::chrono::steady_clock::now() < deadline)
                    std::this_thread::yield();
                cancelledOnDestruction = token.isCancelled();
            });
        }
        check(cancelledOnDestruction, "Job destructor cancels before joining");
        bool cancelledOnReplacement = false;
        auto job = core::async::JobSystem::submit([&](const auto& token) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
            while (!token.isCancelled() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::yield();
            cancelledOnReplacement = token.isCancelled();
        });
        job = core::async::Job{};
        check(cancelledOnReplacement && job.ready(), "Move assignment cancels replaced worker");
        job.wait();
        job.cancel();
        check(!job.cancelled(), "Empty Job has no cancellation state");
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

    // --- Экспорт поверхности губки Менгера в OBJ и binary STL ---
    {
        const auto mesh = core::fractal::MengerMesh::build(1);
        check(!mesh.empty() && mesh.triangleCount() == 144,
              "MengerMesh(1): 20*6 - 24*2 = 72 внешние грани (144 треугольника)");
        const auto cube = core::fractal::MengerMesh::build(0);
        check(cube.triangleCount() == 12, "MengerMesh(0): 12 треугольников куба");
        bool outward = true;
        for (std::size_t i = 0; i < cube.vertices.size(); i += 3) {
            const auto& a = cube.vertices[i];
            const auto& b = cube.vertices[i + 1];
            const auto& c = cube.vertices[i + 2];
            outward = outward && (b - a).cross(c - a).dot(a + b + c) > 0.0;
        }
        check(outward, "Все нормали куба направлены наружу");
        int meshCancellationPolls = 0;
        const auto cancelledMesh = core::fractal::MengerMesh::build(3, [&] {
            return ++meshCancellationPolls >= 2;
        });
        check(cancelledMesh.empty() && meshCancellationPolls >= 2,
              "MengerMesh кооперативно прерывает построение");

        const std::string objPath = "test_menger.obj";
        const std::string stlPath = "test_menger.stl";
        check(core::exporter::MeshExporter::writeObj(objPath, mesh), "MeshExporter::writeObj()");
        check(core::exporter::MeshExporter::writeBinaryStl(stlPath, mesh),
              "MeshExporter::writeBinaryStl()");

        {
            std::ifstream obj(objPath);
            std::string line;
            std::size_t vertices = 0, faces = 0;
            bool validIndices = true;
            while (std::getline(obj, line)) {
                if (line.rfind("v ", 0) == 0) ++vertices;
                if (line.rfind("f ", 0) == 0) {
                    std::istringstream face(line.substr(2));
                    std::size_t a = 0, b = 0, c = 0;
                    validIndices = validIndices && static_cast<bool>(face >> a >> b >> c);
                    validIndices = validIndices && a == faces * 3 + 1 && b == a + 1 && c == b + 1;
                    ++faces;
                }
            }
            check(vertices == mesh.vertices.size() && faces == mesh.triangleCount() && validIndices,
                  "OBJ содержит все вершины и корректные 1-based индексы");
        }
        {
            std::ifstream stl(stlPath, std::ios::binary | std::ios::ate);
            check(stl.tellg() == static_cast<std::streamoff>(84 + 50 * mesh.triangleCount()),
                  "STL: размер 84 + 50*N байт");
            stl.seekg(80);
            unsigned char count[4]{};
            stl.read(reinterpret_cast<char*>(count), 4);
            check(count[0] == 144 && count[1] == 0 && count[2] == 0 && count[3] == 0,
                  "STL: число треугольников в little-endian");
        }
        auto invalid = cube;
        invalid.vertices[0].x = std::numeric_limits<double>::quiet_NaN();
        const bool invalidObjRejected = !core::exporter::MeshExporter::writeObj(objPath, invalid);
        const bool invalidStlRejected =
            !core::exporter::MeshExporter::writeBinaryStl(stlPath, invalid);
        check(invalidObjRejected && invalidStlRejected,
              "Экспорт отклоняет нечисловые координаты");

        std::remove(objPath.c_str());
        std::remove(stlPath.c_str());
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

        int cancellationPolls = 0;
        const auto cancelled = enc.encode(src, 8, 2, [&cancellationPolls] {
            return ++cancellationPolls >= 2;
        });
        check(cancelled.codes.empty() && cancellationPolls >= 2,
              "Encoder кооперативно прерывает длительный поиск доменов");
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
