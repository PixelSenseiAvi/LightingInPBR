#include <stdexcept>
#include <stb_image.h>
#include "image.hpp"

Image::Image() : m_width(0), m_height(0), m_channels(0), m_hdr(false) {}

std::shared_ptr<Image> Image::fromFile(const std::string& filename, int channels)
{
    std::printf("Loading image: %s\n", filename.c_str());

    std::shared_ptr<Image> image = std::make_shared<Image>();

    // Check if the image is HDR
    if (stbi_is_hdr(filename.c_str())) {
        // Load HDR image using stbi_loadf
        float* pixels = stbi_loadf(filename.c_str(), &image->m_width, &image->m_height, &image->m_channels, channels);
        if (pixels) {
            image->m_pixels.reset(reinterpret_cast<unsigned char*>(pixels));
            image->m_hdr = true;
        }
    } 
    else {
        // Load regular image using stbi_load
        unsigned char* pixels = stbi_load(filename.c_str(), &image->m_width, &image->m_height, &image->m_channels, channels);
        if (pixels) {
            image->m_pixels.reset(pixels);
            image->m_hdr = false;
        }
    }

    // Override channel count if channels argument is provided
    if (channels > 0) {
        image->m_channels = channels;
    }

    // Check if the image has been successfully loaded
    if (!image->m_pixels) {
        throw std::runtime_error("Failed to load image file: " + filename);
    }

    return image;
}
