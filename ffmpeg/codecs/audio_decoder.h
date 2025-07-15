#pragma once

extern "C"
{
#include <libavcodec/avcodec.h>
}
#include <sys2/callback.hpp>
#include <memory>

namespace ffmpeg
{
	namespace codecs
	{
		struct audio_decoder_options
		{
			AVCodecID codec_id = AVCodecID::AV_CODEC_ID_NONE;
			int samplerate = 0;
			int channels = 0;
			AVSampleFormat format = AVSampleFormat::AV_SAMPLE_FMT_NONE;
			int thread_count = 1;
		};

		class audio_decoder
		{
		public:
			typedef void (*audio_frame)(void* ctx,const AVFrame* frame);
			audio_decoder(const audio_decoder_options& options);
			virtual ~audio_decoder();

			bool is_opened()const { return ctx_ != nullptr; }
			AVCodecContext* context()const { return ctx_; }

			virtual bool open();
			virtual void close();

			virtual bool send_packet(const AVPacket* packet);
			virtual bool receive_frame(AVFrame* frame);
			virtual bool input(const AVPacket* packet);
		public:
			sys::callback<audio_frame> on_frame;
		private:
			AVCodecContext* ctx_ = nullptr;
			audio_decoder_options options_;
		};

		typedef std::shared_ptr<audio_decoder> audio_decoder_ptr;
	}
}

