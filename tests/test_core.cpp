#include <cmath>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "core/async/JobSystem.h"
#include "core/fractal/Fractal.h"
#include "core/fractal/Julia3D.h"
#include "core/fractal/Mandelbulb3D.h"
#include "core/fractal/Mandelbrot2D.h"
#include "core/fractal/MengerSponge.h"
#include "core/fractal/SphericalDistance.h"
#include "core/math/Complex.h"
#include "core/math/Vector3.h"
#include "utils/Config.h"

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

int main() {
    std::cout << "=== ars_fracta: test_core ===\n\n";

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

        // --- sanitizeParams: единая точка валидации по типу ---
        {
            FractalParams dirty;
            dirty.type = FractalType::Mandelbrot2D;
            dirty.iterations = 4;   // ниже допустимого минимума
            const FractalParams clean = core::fractal::sanitizeParams(dirty);
            check(clean.type == FractalType::Mandelbrot2D && clean.iterations >= 16,
                  "sanitizeParams(Mandelbrot2D) корректирует параметры");

            FractalParams terrain;
            terrain.type = FractalType::Terrain3D;
            check(core::fractal::sanitizeParams(terrain).type == FractalType::Terrain3D,
                  "sanitizeParams(Terrain3D) не трогает shader-тип");

            // автовращение — UI-режим только 3D-фракталов (0..3): для пейзажа/
            // побережья sanitizeParams принудительно сбрасывает флаг даже если он
            // доехал из config.json или пресета (см. MainWindow::orbit gate).
            FractalParams terrainRot;
            terrainRot.type = FractalType::Terrain3D;
            terrainRot.autoRotate = true;
            check(!core::fractal::sanitizeParams(terrainRot).autoRotate,
                  "sanitizeParams(Terrain3D) сбрасывает autoRotate");

            FractalParams coastRot;
            coastRot.type = FractalType::Coast3D;
            coastRot.autoRotate = true;
            check(!core::fractal::sanitizeParams(coastRot).autoRotate,
                  "sanitizeParams(Coast3D) сбрасывает autoRotate");

            FractalParams bulbRot;
            bulbRot.type = FractalType::Mandelbulb3D;
            bulbRot.autoRotate = true;
            check(core::fractal::sanitizeParams(bulbRot).autoRotate,
                  "sanitizeParams(Mandelbulb3D) оставляет autoRotate включённым");
        }

        std::ifstream shader("assets/shaders/fractal.frag");
        const std::string shaderText((std::istreambuf_iterator<char>(shader)),
                                     std::istreambuf_iterator<char>());
        check(!shaderText.empty() && shaderText.find("uFractalType == 4") != std::string::npos &&
                  shaderText.find("uFractalType == 5") != std::string::npos &&
                  shaderText.find("renderTerrain") != std::string::npos &&
                  shaderText.find("renderCoast") != std::string::npos,
              "Terrain3D и Coast3D имеют shader-путь генерации");
        // красная зона: плотность тумана параметризована uniform'ом, а не литералом
        check(shaderText.find("uniform float uFogDensity;") != std::string::npos &&
                  shaderText.find("-dist * uFogDensity") != std::string::npos &&
                  shaderText.find("0.0055f") == std::string::npos,
              "Шейдер читает плотность тумана из uFogDensity (литерал 0.0055 удалён)");
    }

    // --- JSON round-trip FractalParams (красная зона FractalParams <-> JSON) ---
    {
        using core::fractal::FractalParams;
        using core::fractal::FractalType;
        check(FractalParams{}.fogDensity == 0.0055f,
              "fogDensity по умолчанию == 0.0055 (как в шейдере до параметризации)");

        FractalParams p;
        p.type = FractalType::Terrain3D;
        p.fogDensity = 0.0135f;
        const std::string path = "test_fog_roundtrip.json";
        check(utils::Config::saveFractal(path, p), "Config::saveFractal() записала файл");
        FractalParams loaded;
        check(utils::Config::loadFractal(path, loaded), "Config::loadFractal() прочитала файл");
        check(loaded.fogDensity == 0.0135f && loaded == p,
              "JSON round-trip: fogDensity сохранён, все поля равны");
        std::remove(path.c_str());
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

    // --- Барьер дедлока: wait() из самого worker'а бросает logic_error ---
    {
        using core::async::Job;
        std::atomic<Job*> selfHandle{nullptr};
        std::atomic_bool guardTriggered{false};
        auto job = core::async::JobSystem::submit(
            [&selfHandle, &guardTriggered](const auto&) {
                const auto deadline = std::chrono::steady_clock::now() +
                                      std::chrono::milliseconds(500);
                while (selfHandle.load(std::memory_order_acquire) == nullptr &&
                       std::chrono::steady_clock::now() < deadline)
                    std::this_thread::yield();
                if (selfHandle.load(std::memory_order_acquire) == nullptr) return;
                try {
                    selfHandle.load(std::memory_order_acquire)->wait();
                } catch (const std::logic_error&) {
                    guardTriggered.store(true, std::memory_order_release);
                }
            });
        selfHandle.store(&job, std::memory_order_release);
        job.wait();
        check(guardTriggered.load(std::memory_order_acquire),
              "Job::wait() из worker'а бросает logic_error вместо зависания");
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

    // --- Общий сферический DE: обёртки Мандельбульба/Жюлиа форвардят в helper ---
    {
        using core::math::Vector3;
        const Vector3 starts[] = {Vector3(0, 0, 0), Vector3(0.1, 0.2, 0.3), Vector3(1, -2, 3)};
        const double powers[] = {2.0, 8.0, 15.0};
        const int iters[] = {8, 32, 64};
        bool bulbMatches = true, juliaMatches = true;
        for (const Vector3& p : starts) {
            for (const double power : powers) {
                for (const int n : iters) {
                    bulbMatches = bulbMatches &&
                        core::fractal::Mandelbulb3D::distanceEstimate(p, power, n) ==
                        core::fractal::sphericalDistanceEstimate(p, p, power, n);
                    const Vector3 c(0.5, -0.25, 0.1);
                    juliaMatches = juliaMatches &&
                        core::fractal::Julia3D::distanceEstimate(p, c, power, n) ==
                        core::fractal::sphericalDistanceEstimate(p, c, power, n);
                }
            }
        }
        check(bulbMatches, "Mandelbulb3D DE == sphericalDistanceEstimate(p, p)");
        check(juliaMatches, "Julia3D DE == sphericalDistanceEstimate(p, c)");
    }

    std::cout << "\n=== Итог: " << failures << " ошибок ===\n";
    return failures == 0 ? 0 : 1;
}
