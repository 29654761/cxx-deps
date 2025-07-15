#pragma once

extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

namespace ffmpeg
{
    namespace utils
    {

        class filters
        {
        public:
            filters();
            ~filters();

            bool open();
            void close();
            bool is_active()const;
            bool is_changed(int width, int height, AVPixelFormat format)const;
            bool is_changed(const AVFrame* frame)const;

            bool add_in_buffer(int width, int height, AVPixelFormat format);
            bool add_out_buffer();

            bool add_hflip();
            bool add_vflip();
            bool add_overlay();

            /// <summary>
            /// 
            /// </summary>
            /// <param name="dir">
            /// 0 - Rotate by 90 degrees counterclockwise and vertically flip (default)
            /// 1 - Rotate by 90 degrees clockwise,
            /// 2 - Rotate by 90 degrees counterclockwise,
            /// 3 - Rotate by 90 degrees clockwise and vertically flip
            /// </param>
            /// <param name="passthrough">
            /// none - Always apply transposition. (default)
            /// portrait - Preserve portrait geometry (when height >= width).
            /// landscape - Preserve landscape geometry (when width >= height).
            /// </param>
            /// <returns></returns>
            bool add_transpose(int dir, const char* passthrough = "none");


            bool add_frame(AVFrame* frame);
            bool get_frame(AVFrame* frame);
        private:
            int width_ = 0;
            int height_ = 0;
            AVPixelFormat format_ = AVPixelFormat::AV_PIX_FMT_NONE;

            AVFilterGraph* filter_graph_ = nullptr;
            AVFilterContext* in_filter_ = nullptr;
            AVFilterContext* out_filter_ = nullptr;
            AVFilterContext* pre_filter_ = nullptr;
        };

    }
}

