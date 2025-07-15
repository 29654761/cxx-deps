#include "video_decoder_soft.h"



namespace ffmpeg
{
	namespace codecs
	{

		video_decoder_soft::video_decoder_soft(const video_decoder_options& options)
			:video_decoder(options)
		{
			
		}

		video_decoder_soft::~video_decoder_soft()
		{
			close();
		}

		bool video_decoder_soft::open()
		{
			const AVCodec* codec = avcodec_find_decoder(options_.codec_id);
			if (!codec)
			{
				close();
				return false;
			}
			context_ = avcodec_alloc_context3(codec);
			if (!context_)
			{
				close();
				return false;
			}
			context_->thread_count = options_.thread_count;
			int r = avcodec_open2(context_, codec, nullptr);
			if (r != 0)
			{
				close();
				return false;
			}

			return true;
		}

		void video_decoder_soft::close()
		{
			video_decoder::close();
		}


		bool video_decoder_soft::send_packet(const AVPacket* packet)
		{
			return video_decoder::send_packet(packet);
		}

		bool video_decoder_soft::receive_frame(AVFrame* frame)
		{
			return video_decoder::receive_frame(frame);
		}
	}
}

