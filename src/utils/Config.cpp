#include "utils/Config.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace ars::utils {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
//  FractalParams <-> JSON helpers
// ---------------------------------------------------------------------------

namespace {

std::string typeToString(core::fractal::FractalType t) {
    switch (t) {
        case core::fractal::FractalType::Mandelbrot2D: return "Mandelbrot2D";
        case core::fractal::FractalType::Mandelbulb3D: return "Mandelbulb3D";
        case core::fractal::FractalType::MengerSponge: return "MengerSponge";
        case core::fractal::FractalType::Julia3D:      return "Julia3D";
        case core::fractal::FractalType::Terrain3D:    return "Terrain3D";
    }
    return "Mandelbulb3D";
}

core::fractal::FractalType typeFromString(const std::string& s) {
    if (s == "Mandelbrot2D") return core::fractal::FractalType::Mandelbrot2D;
    if (s == "Mandelbulb3D") return core::fractal::FractalType::Mandelbulb3D;
    if (s == "MengerSponge") return core::fractal::FractalType::MengerSponge;
    if (s == "Julia3D")      return core::fractal::FractalType::Julia3D;
    if (s == "Terrain3D")    return core::fractal::FractalType::Terrain3D;
    return core::fractal::FractalType::Mandelbulb3D;
}

json fractalToJson(const core::fractal::FractalParams& p) {
    json j;
    j["type"] = typeToString(p.type);
    j["iterations"] = p.iterations;
    j["bailout"] = p.bailout;
    j["power"] = p.power;
    j["juliaReal"] = p.juliaReal;
    j["juliaImag"] = p.juliaImag;
    j["juliaImag3D"] = p.juliaImag3D;
    j["detail"] = p.detail;
    j["colorScale"] = p.colorScale;
    j["hueShift"] = p.hueShift;
    j["colorMode"] = p.colorMode;
    j["m2dZoom"] = p.m2dZoom;
    j["terrainAmplitude"] = p.terrainAmplitude;
    j["terrainFrequency"] = p.terrainFrequency;
    j["cloudDensity"] = p.cloudDensity;
    j["treeDensity"] = p.treeDensity;
    j["timeOfDay"] = p.timeOfDay;
    j["autoRotate"] = p.autoRotate;
    return j;
}

core::fractal::FractalParams fractalFromJson(const json& j) {
    core::fractal::FractalParams p;
    if (j.contains("type"))        p.type = typeFromString(j["type"].get<std::string>());
    if (j.contains("iterations"))  p.iterations = j["iterations"].get<int>();
    if (j.contains("bailout"))     p.bailout = j["bailout"].get<float>();
    if (j.contains("power"))       p.power = j["power"].get<float>();
    if (j.contains("juliaReal"))   p.juliaReal = j["juliaReal"].get<float>();
    if (j.contains("juliaImag"))   p.juliaImag = j["juliaImag"].get<float>();
    if (j.contains("juliaImag3D")) p.juliaImag3D = j["juliaImag3D"].get<float>();
    if (j.contains("detail"))      p.detail = j["detail"].get<float>();
    if (j.contains("colorScale"))  p.colorScale = j["colorScale"].get<float>();
    if (j.contains("hueShift"))    p.hueShift = j["hueShift"].get<float>();
    if (j.contains("colorMode"))   p.colorMode = j["colorMode"].get<int>();
    if (j.contains("m2dZoom"))     p.m2dZoom = j["m2dZoom"].get<float>();
    if (j.contains("terrainAmplitude")) p.terrainAmplitude = j["terrainAmplitude"].get<float>();
    if (j.contains("terrainFrequency")) p.terrainFrequency = j["terrainFrequency"].get<float>();
    if (j.contains("cloudDensity"))     p.cloudDensity = j["cloudDensity"].get<float>();
    if (j.contains("treeDensity"))      p.treeDensity = j["treeDensity"].get<float>();
    if (j.contains("timeOfDay"))        p.timeOfDay = j["timeOfDay"].get<float>();
    if (j.contains("autoRotate"))  p.autoRotate = j["autoRotate"].get<bool>();
    return p;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

bool Config::save(const std::string& filePath, const ConfigData& data) {
    try {
        json j;
        j["windowWidth"] = data.windowWidth;
        j["windowHeight"] = data.windowHeight;
        j["fractal"] = fractalToJson(data.fractal);
        std::ofstream f(filePath);
        if (!f) return false;
        f << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] save failed: " << e.what() << '\n';
        return false;
    }
}

bool Config::load(const std::string& filePath, ConfigData& data) {
    std::ifstream f(filePath);
    if (!f) return false;
    try {
        json j;
        f >> j;
        if (j.contains("windowWidth"))  data.windowWidth = j["windowWidth"].get<int>();
        if (j.contains("windowHeight")) data.windowHeight = j["windowHeight"].get<int>();
        if (j.contains("fractal"))      data.fractal = fractalFromJson(j["fractal"]);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] load failed: " << e.what() << '\n';
        return false;
    }
}

bool Config::loadFractal(const std::string& filePath, core::fractal::FractalParams& out) {
    std::ifstream f(filePath);
    if (!f) return false;
    try {
        json j;
        f >> j;
        out = fractalFromJson(j);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] loadFractal failed: " << e.what() << '\n';
        return false;
    }
}

bool Config::saveFractal(const std::string& filePath, const core::fractal::FractalParams& p) {
    try {
        std::ofstream f(filePath);
        if (!f) return false;
        f << fractalToJson(p).dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace ars::utils