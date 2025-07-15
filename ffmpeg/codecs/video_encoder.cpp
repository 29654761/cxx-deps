
#include "video_encoder.h"


namespace ffmpeg
{
	namespace codecs
	{

		video_encoder::video_encoder(const video_encoder_options& options)
		{
            options_ = options;
		}

		video_encoder::~video_encoder()
		{
		}

		


		void video_encoder::close()
		{
            if (context_)
            {
                avcodec_send_frame(context_, nullptr);
                avcodec_close(context_);
                avcodec_free_context(&context_);
            }
		}

		bool video_encoder::input(const AVFrame* frame)
		{
            if (!send_frame(frame))
            {
                return false;
            }

            AVPacket pkt = {};
            while (receive_packet(&pkt))
            {
                on_packet.invoke(&pkt);
                av_packet_unref(&pkt);
            }
            return true;
		}

		bool video_encoder::send_frame(const AVFrame* frame)
		{
            if (keyframe_)
            {
                ((AVFrame*)frame)->pict_type = AVPictureType::AV_PICTURE_TYPE_I;
                keyframe_ = false;
            }
            else
            {
                ((AVFrame*)frame)->pict_type = AVPictureType::AV_PICTURE_TYPE_NONE;
            }

            //av_hwframe_transfer_data
            int r = avcodec_send_frame(context_, frame);
            //char err[64] = {};
            //av_make_error_string(err, 64, r);
            return r >= 0;
		}

		bool video_encoder::receive_packet(AVPacket* packet)
		{
            int r = avcodec_receive_packet(context_, packet);
            if (r < 0)
            {
                return false;
            }

            packet->time_base = context_->time_base;

            return true;
		}

        void video_encoder::required_keyframe()
        {
            keyframe_ = true;
        }
	}
}

