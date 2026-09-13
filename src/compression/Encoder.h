#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "utils/ImageLoader.h"

namespace ars::compression {

/**
 * @brief Код одного range-блока фрактального сжатия (IFS по Жакину).
 *
 * Кодирование: к каждому range-блоку (8x8) подбирается домен (16x16,
 * уменьшенный к 8x8), одна из 8 симметрий и аффинное преобразование
 * яркости: y = scale * domain + offset.
 */
struct BlockCode {
    uint16_t rangeX = 0;   // координата range-блока (в пикселях)
    uint16_t rangeY = 0;
    uint16_t domainX = 0;  // координата домена (в пикселях)
    uint16_t domainY = 0;
    uint8_t  isometry = 0; // 0..7 — тождество/повороты/отражения
    float    scale = 0.0f; // контраст s
    float    offset = 0.0f; // яркость o
};

/**
 * @brief Алгоритм сжатия (кодер).
 */
class Encoder {
public:
    struct EncodedImage {
        int width = 0;
        int height = 0;
        int blockSize = 8;     // размер range-блока
        int domainScale = 2;   // домен в domainScale раз больше

        std::vector<BlockCode> codes;

        size_t byteSize() const;
        bool writeFile(const std::string& path) const;
        static bool readFile(const std::string& path, EncodedImage& out);
    };

    /**
     * @brief Кодирование полутонового изображения (grayscale, channels == 1).
     */
    EncodedImage encode(const utils::Image& grayscale, int blockSize = 8,
                        int domainScale = 2);
};

} // namespace ars::compression