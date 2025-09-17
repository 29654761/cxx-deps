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

            if (inframe->width == dst_width_ && inframe->height == dst_height_ && inframe->format == dst_format_)
                return av_frame_clone(inframe);

            AVFrame* ret = av_frame_alloc();
            if (!ret)
            {
                return nullptr;
            }

            int r = sws_scale_frame(ctx_, ret, inframe);
            if (r < 0)
            {
                return nullptr;
            }

            av_frame_copy_props(ret, inframe);
            ret->width = dst_width_;
            ret->height = dst_height_;
            ret->format = (int)dst_format_;
            return ret;
        }
    }
}

