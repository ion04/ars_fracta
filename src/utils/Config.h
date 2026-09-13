#pragma once

#include <string>

#include "core/fractal/Fractal.h"

namespace ars::utils {

/**
 * @brief Данные конфигурации приложения (сохраняются/загружаются из config.json).
 */
struct ConfigData {
    int windowWidth = 1280;
    int windowHeight = 720;
    core::fractal::FractalParams fractal;
};

/**
 * @brief Загрузка / сохранение конфигурации и пресетов фракталов (JSON).
 */
class Config {
public:
    static bool save(const std::string& filePath, const ConfigData& data);
    static bool load(const std::string& filePath, ConfigData& data);

    /// Загрузить параметры фрактала из JSON-файла (пресет).
    static bool loadFractal(const std::string& filePath,
                            core::fractal::FractalParams& out);

    /// Сохранить параметры фрактала в JSON-файл.
    static bool saveFractal(const std::string& filePath,
                            const core::fractal::FractalParams& params);
};

} // namespace ars::utils