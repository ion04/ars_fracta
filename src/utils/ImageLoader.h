#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ars::utils {

/**
 * @brief Простейшее изображение: массив байтов (0..255) + размеры + кол-во каналов.
 */
struct Image {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> data;
};

/**
 * @brief Загрузка/сохранение изображений через stb_image.
 */
class ImageLoader {
public:
    /// Загрузка PNG/JPG/BMP (stb определяет формат автоматически).
    static bool load(const std::string& path, Image& out);

    /// Сохранение в PNG.
    static bool savePng(const std::string& path, const Image& img);

    /// Конвертация в 1-канальное изображение (усреднение каналов).
    static Image toGrayscale(const Image& in);
};

} // namespace ars::utils