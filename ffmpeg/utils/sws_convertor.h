#pragma once
extern "C" 
{
#include <libswscale/swscale.h>
}

namespace ffmpeg
{
    namespace utils
    {

        class sws_convertor
        {
        public:
            sws_convertor();
            ~sws_convertor();

            bool is_opened()
            {
                return ctx_ ? true : false;
            }

            bool is_changed(const AVFrame* frame)
            {
                return src_width_ != frame->width || src_height_ != frame->height || src_format_ != frame->format;
            }

            void ensure_created(const AVFrame* frame, int dst_width, int dst_height, AVPixelFormat dst_format);

            bool open(int src_width, int src_height, AVPixelFormat src_format,
                int dst_width, int dst_height, AVPixelFormat dst_format);

            void close();

            AVFrame* convert(const AVFrame* inframe);


        private:
            SwsContext* ctx_ = nullptr;
            int src_width_ = 0;
            int src_height_ = 0;
            AVPixelFormat src_format_ = AVPixelFormat::AV_PIX_FMT_NONE;
            int dst_width_ = 0;
            int dst_height_ = 0;
            AVPixelFormat dst_format_ = AVPixelFormat::AV_PIX_FMT_NONE;
            AVFrame* frame_ = nullptr;
        };

    }
}

