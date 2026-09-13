#include "utils/ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cstring>
#include <iostream>

namespace ars::utils {

bool ImageLoader::load(const std::string& path, Image& out) {
    int w = 0, h = 0, ch = 0;
    unsigned char* p = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (!p) {
        std::cerr << "[ImageLoader] stbi_load failed: " << stbi_failure_reason() << '\n';
        return false;
    }
    out.width = w;
    out.height = h;
    out.channels = ch;
    out.data.assign(p, p + static_cast<std::size_t>(w) * h * ch);
    stbi_image_free(p);
    return true;
}

bool ImageLoader::savePng(const std::string& path, const Image& img) {
    if (img.data.empty() || img.width <= 0 || img.height <= 0) return false;
    const int stride = img.width * img.channels;
    return stbi_write_png(path.c_str(), img.width, img.height, img.channels,
                          img.data.data(), stride) != 0;
}

Image ImageLoader::toGrayscale(const Image& in) {
    Image out;
    out.width = in.width;
    out.height = in.height;
    out.channels = 1;
    const std::size_t npix = static_cast<std::size_t>(in.width) * in.height;
    out.data.resize(npix);

    const int c = in.channels > 0 ? in.channels : 1;
    const int used = std::min(c, 3); // игнорируем альфа-канал при усреднении
    if (c == 1) {
        out.data = in.data;
    } else {
        for (std::size_t i = 0; i < npix; ++i) {
            std::uint32_t sum = 0;
            for (int k = 0; k < used; ++k) {
                sum += in.data[i * c + k];
            }
            out.data[i] = static_cast<std::uint8_t>(sum / static_cast<std::uint32_t>(used));
        }
    }
    return out;
}

} // namespace ars::utils