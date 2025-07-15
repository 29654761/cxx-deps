#include "bit_stream_filter.h"

namespace ffmpeg
{
    namespace utils
    {

        bit_stream_filter::bit_stream_filter()
        {
        }

        bit_stream_filter::~bit_stream_filter()
        {
            close();
        }

        bool bit_stream_filter::open(const AVCodecContext* codec, const std::string& bsf_name)
        {
            if (is_opened())
            {
                return false;
            }
            const AVBitStreamFilter* filter = av_bsf_get_by_name(bsf_name.c_str());
            if (!filter)
                return false;

            int r = av_bsf_alloc(filter, &context_);
            if (r < 0)
            {
                close();
                return false;
            }
            r = avcodec_parameters_from_context(context_->par_in, codec);
            if (r < 0)
            {
                close();
                return false;
            }

            r = av_bsf_init(context_);
            if (r < 0)
            {
                close();
                return false;
            }
            return true;

            return true;
        }

        bool bit_stream_filter::open(const AVCodecParameters* par, const std::string& bsf_name)
        {
            if (is_opened())
            {
                return false;
            }
            const AVBitStreamFilter* filter = av_bsf_get_by_name(bsf_name.c_str());
            if (!filter)
                return false;

            int r = av_bsf_alloc(filter, &context_);
            if (r < 0)
            {
                close();
                return false;
            }

            r=avcodec_parameters_copy(context_->par_in, par);
            if (r < 0)
            {
                close();
                return false;
            }

            r = av_bsf_init(context_);
            if (r < 0)
            {
                close();
                return false;
            }
            return true;
        }

        void bit_stream_filter::close()
        {
            if (context_)
            {
                av_bsf_free(&context_);
            }
        }

        bool bit_stream_filter::send_packet(AVPacket* packet)
        {
            if (!context_)
            {
                return false;
            }

            int r = av_bsf_send_packet(context_, packet);
            return r >= 0;
        }

        bool bit_stream_filter::receive_packet(AVPacket* packet)
        {
            if (!context_)
            {
                return false;
            }
            int r = av_bsf_receive_packet(context_, packet);
            return r >= 0;
        }


    }
}

