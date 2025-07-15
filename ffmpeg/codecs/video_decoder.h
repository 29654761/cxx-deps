#pragma once

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include <sys2/callback.hpp>
#include <memory>


namespace ffmpeg
{
	namespace codecs
	{
		struct video_decoder_options
		{
			AVCodecID codec_id = AVCodecID::AV_CODEC_ID_NONE;
			int thread_count = 1;
		};

		class video_decoder
		{
		public:
			typedef void (*video_frame)(void* ctx, AVFrame* frame);

			video_decoder(const video_decoder_options& options);
			virtual ~video_decoder();

			bool is_opened()const { return context_; }


			virtual bool open() = 0;
			virtual void close();

			virtual bool input(const AVPacket* packet);
			virtual bool send_packet(const AVPacket* packet);
			virtual bool receive_frame(AVFrame* frame);

		public:
			sys::callback<video_frame> on_frame;
		protected:
			AVCodecContext* context_ = nullptr;
			video_decoder_options options_;
		};


		typedef std::shared_ptr<video_decoder> video_decoder_ptr;
	}
}
