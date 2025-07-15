#include "sws_convertor.h"

namespace ffmpeg
{
    namespace utils
    {

        sws_convertor::sws_convertor()
        {

        }

        sws_convertor::~sws_convertor()
        {
            close();
        }

        bool sws_convertor::open(int src_width, int src_height, AVPixelFormat src_format,
            int dst_width, int dst_height, AVPixelFormat dst_format)
        {
            if (is_opened())
            {
                return false;
            }

            src_width_ = src_width;
            src_height_ = src_height;
            src_format_ = src_format;
            dst_width_ = dst_width;
            dst_height_ = dst_height;
            dst_format_ = dst_format;

            frame_ = av_frame_alloc();
            if (!frame_)
            {
                close();
                return false;
            }

            ctx_ = sws_getContext(src_width_, src_height_, src_format_,
                dst_width, dst_height, dst_format,
                SWS_BILINEAR, nullptr, nullptr, nullptr);

            if (!ctx_)
            {
                close();
                return false;
            }

            return true;
        }

        void sws_convertor::close()
        {
            if (ctx_)
            {
                sws_freeContext(ctx_);
                ctx_ = nullptr;
            }
            if (frame_)
            {
                av_frame_free(&frame_);
            }
        }

        void sws_convertor::ensure_created(const AVFrame* frame, int dst_width, int dst_height, AVPixelFormat dst_format)
        {
            if (is_changed(frame))
            {
                close();
            }
            if (!is_opened())
            {
                open(frame->width, frame->height, (AVPixelFormat)frame->format,
                    dst_width, dst_height, dst_format);
            }
        }

        AVFrame* sws_convertor::convert(const AVFrame* inframe)
        {
            if (!ctx_)
            {
                return nullptr;
            }

            int r = sws_scale_frame(ctx_, frame_, inframe);
            if (r < 0)
            {
                return nullptr;
            }
            frame_->width = dst_width_;
            frame_->height = dst_height_;
            frame_->format = (int)dst_format_;
            return frame_;
        }
    }
}

