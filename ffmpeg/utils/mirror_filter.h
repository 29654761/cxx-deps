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

        class mirror_filter
        {
        public:

            enum class direction_t
            {
                hflip,
                vflip,
            };

            mirror_filter();
            ~mirror_filter();

            bool open(int width, int height, AVPixelFormat format, direction_t dir);
            void close();
            bool is_opened();
            bool is_changed(const AVFrame* frame);
            void ensure_created(const AVFrame* frame, direction_t dir);

            bool add_frame(AVFrame* frame);
            bool get_frame(AVFrame* frame);
        private:
            int width_ = 0;
            int height_ = 0;
            AVPixelFormat format_;

            AVFilterGraph* filter_graph_ = NULL;
            AVFilterContext* filter_in_ = NULL;
            AVFilterContext* filter_out_ = NULL;
        };

    }
}

