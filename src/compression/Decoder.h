#pragma once

#include "compression/Encoder.h"
#include "utils/ImageLoader.h"

namespace ars::compression {

/**
 * @brief Восстановление изображения из фрактального кода.
 *
 * Итеративная схема: на каждом шаге текущее изображение отображается через
 * сохранённые контрактивные отображения в новое; процесс сходится к
 * неподвижной точке — восстановленному изображению.
 */
class Decoder {
public:
    static utils::Image decode(const Encoder::EncodedImage& encoded, int iterations = 8);
};

} // namespace ars::compression