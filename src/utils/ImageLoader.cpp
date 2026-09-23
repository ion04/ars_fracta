#include "utils/ImageLoader.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace ars::utils {

bool ImageLoader::savePng(const std::string& path, const Image& img) {
    if (img.data.empty() || img.width <= 0 || img.height <= 0) return false;
    const int stride = img.width * img.channels;
    return stbi_write_png(path.c_str(), img.width, img.height, img.channels,
                          img.data.data(), stride) != 0;
}

} // namespace ars::utils