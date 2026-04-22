#include "image_resize.h"

float ImageResize::bilinearInterpolate(const uint8_t* data,
                                        int width, int height, int channels,
                                        float x, float y, int c) {
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    
    x0 = (x0 < 0) ? 0 : (x0 >= width - 1 ? width - 1 : x0);
    x1 = (x1 >= width) ? width - 1 : x1;
    y0 = (y0 < 0) ? 0 : (y0 >= height - 1 ? height - 1 : y0);
    y1 = (y1 >= height) ? height - 1 : y1;
    
    float dx = x - x0;
    float dy = y - y0;
    
    float v00 = static_cast<float>(data[(y0 * width + x0) * channels + c]);
    float v10 = static_cast<float>(data[(y0 * width + x1) * channels + c]);
    float v01 = static_cast<float>(data[(y1 * width + x0) * channels + c]);
    float v11 = static_cast<float>(data[(y1 * width + x1) * channels + c]);
    
    float v0 = v00 * (1.0f - dx) + v10 * dx;
    float v1 = v01 * (1.0f - dx) + v11 * dx;
    
    return v0 * (1.0f - dy) + v1 * dy;
}

void ImageResize::resizeRgb(const uint8_t* src_data,
                             int src_width, int src_height,
                             int dst_width, int dst_height,
                             std::vector<uint8_t>& dst_data) {
    dst_data.resize(dst_width * dst_height * 3);
    
    float scale_x = static_cast<float>(src_width) / dst_width;
    float scale_y = static_cast<float>(src_height) / dst_height;
    
    for (int y = 0; y < dst_height; ++y) {
        for (int x = 0; x < dst_width; ++x) {
            float src_x = (x + 0.5f) * scale_x - 0.5f;
            float src_y = (y + 0.5f) * scale_y - 0.5f;
            
            int dst_idx = (y * dst_width + x) * 3;
            
            for (int c = 0; c < 3; ++c) {
                float val = bilinearInterpolate(src_data, src_width, src_height, 3, src_x, src_y, c);
                dst_data[dst_idx + c] = static_cast<uint8_t>(val < 0 ? 0 : (val > 255 ? 255 : val));
            }
        }
    }
}

void ImageResize::resizeGrayscale(const uint8_t* src_data,
                                   int src_width, int src_height,
                                   int dst_width, int dst_height,
                                   std::vector<uint8_t>& dst_data) {
    dst_data.resize(dst_width * dst_height);
    
    float scale_x = static_cast<float>(src_width) / dst_width;
    float scale_y = static_cast<float>(src_height) / dst_height;
    
    for (int y = 0; y < dst_height; ++y) {
        for (int x = 0; x < dst_width; ++x) {
            float src_x = (x + 0.5f) * scale_x - 0.5f;
            float src_y = (y + 0.5f) * scale_y - 0.5f;
            
            float val = bilinearInterpolate(src_data, src_width, src_height, 1, src_x, src_y, 0);
            dst_data[y * dst_width + x] = static_cast<uint8_t>(val < 0 ? 0 : (val > 255 ? 255 : val));
        }
    }
}