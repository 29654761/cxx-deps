#ifndef IMAGE_RESIZE_H
#define IMAGE_RESIZE_H

#include <vector>
#include <cstdint>

class ImageResize {
public:
    static void resizeRgb(const uint8_t* src_data,
                          int src_width, int src_height,
                          int dst_width, int dst_height,
                          std::vector<uint8_t>& dst_data);
    
    static void resizeGrayscale(const uint8_t* src_data,
                                 int src_width, int src_height,
                                 int dst_width, int dst_height,
                                 std::vector<uint8_t>& dst_data);

private:
    static inline float bilinearInterpolate(const uint8_t* data,
                                            int width, int height, int channels,
                                            float x, float y, int c);
};

#endif