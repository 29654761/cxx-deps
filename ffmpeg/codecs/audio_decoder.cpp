#include "audio_decoder.h"

namespace ffmpeg
{
	namespace codecs
	{

		audio_decoder::audio_decoder(const audio_decoder_options& options)
		{
			options_ = options;
		}

		audio_decoder::~audio_decoder()
		{
			close();
		}

		bool audio_decoder::open()
		{
			if (is_opened())
			{
				return false;
			}
			

			const AVCodec* codec = avcodec_find_decoder(options_.codec_id);
			if (!codec)
			{
				printf("avcodec_find_decoder error:codec=%d\n", options_.codec_id);
				close();
				return false;
			}

			ctx_ = avcodec_alloc_context3(codec);
			if (!ctx_)
			{
				close();
				return false;
			}

			ctx_->sample_fmt = options_.format;
			ctx_->sample_rate = options_.samplerate;
			av_channel_layout_default(&ctx_->ch_layout, options_.channels);
			ctx_->thread_count = options_.thread_count;
			int r = avcodec_open2(ctx_, codec, nullptr);
			if (r < 0)
			{
				char err[100] = {};
				av_make_error_string(err,99,r);
				printf("avcodec_open2 error:err=%s\n", err);
				close();
				return false;
			}

			return true;
		}

		void audio_decoder::close()
		{
			if (ctx_)
			{
				avcodec_close(ctx_);
				avcodec_free_context(&ctx_);
			}
		}

		bool audio_decoder::send_packet(const AVPacket* packet)
		{
			if (!is_opened())
			{
				return false;
			}
			
			int r = avcodec_send_packet(ctx_, packet);
			return r >= 0;
		}

		bool audio_decoder::receive_frame(AVFrame* frame)
		{
			if (!is_opened())
			{
				return false;
			}
			int r = avcodec_receive_frame(ctx_, frame);
			if (r < 0)
			{
				return false;
			}

			frame->time_base = this->ctx_->time_base;
			return true;
		}


		bool audio_decoder::input(const AVPacket* packet)
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

	}
}

