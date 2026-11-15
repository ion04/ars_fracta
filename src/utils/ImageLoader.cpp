#include "utils/ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace ars::utils {

bool ImageLoader::loadPng(const std::string& path, Image& img) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) return false;
    img.width = width;
    img.height = height;
    img.channels = 4;
    img.data.assign(data, data + static_cast<std::size_t>(width) * height * 4);
    stbi_image_free(data);
    return true;
}

bool ImageLoader::savePng(const std::string& path, const Image& img) {
    if (img.data.empty() || img.width <= 0 || img.height <= 0) return false;
    const int stride = img.width * img.channels;
    return stbi_write_png(path.c_str(), img.width, img.height, img.channels,
                          img.data.data(), stride) != 0;
}

} // namespace ars::utils