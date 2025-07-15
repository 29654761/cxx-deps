#include "video_decoder.h"


namespace ffmpeg
{
    namespace codecs
    {

        video_decoder::video_decoder(const video_decoder_options& options)
        {
            options_ = options;
        }

        video_decoder::~video_decoder()
        {
            close();
        }

        void video_decoder::close()
        {
            if (context_)
            {
                avcodec_close(context_);
                avcodec_free_context(&context_);
            }
        }

        bool video_decoder::input(const AVPacket* packet)
        {
            if (!send_packet(packet))
            {
                return false;
            }


            AVFrame frame = {};
            while (receive_frame(&frame))
            {
                on_frame.invoke(&frame);
                av_frame_unref(&frame);
            }


            return true;
        }


        bool video_decoder::send_packet(const AVPacket* packet)
        {
            int r = avcodec_send_packet(context_, packet);
            return r >= 0;
        }

        bool video_decoder::receive_frame(AVFrame* frame)
        {
            int r = avcodec_receive_frame(context_, frame);
            return r >= 0;
        }

    }
}

