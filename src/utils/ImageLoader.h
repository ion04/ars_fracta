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
 * @brief Сохранение изображений в PNG (stb_image_write).
 */
class ImageLoader {
public:
    /// Сохранение в PNG.
    static bool savePng(const std::string& path, const Image& img);
};

} // namespace ars::utils