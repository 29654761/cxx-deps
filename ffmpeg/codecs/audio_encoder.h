#pragma once

#include "../utils/audio_fifo.h"

extern "C"
{
#include <libavcodec/avcodec.h>
}

#include <sys2/callback.hpp>
#include <atomic>
#include <memory>
#include <string>

namespace ffmpeg
{
	namespace codecs
	{
		struct audio_encoder_options
		{
			AVCodecID codec_id = AVCodecID::AV_CODEC_ID_NONE;
			int samplerate = 0;
			int channels = 0;
			AVSampleFormat format = AVSampleFormat::AV_SAMPLE_FMT_NONE;
			int thread_count = 1;
			int frame_size = 0;

			bool opus_inband_fec = false;
			int  opus_fec_packet_loss = 0;  //0~100%   Packet loss
		};

		class audio_encoder
		{
		public:
			typedef void (*audio_packet)(void* ctx,const AVPacket* packet, const AVFrame* encframe);

			audio_encoder(const audio_encoder_options& options);
			virtual ~audio_encoder();

			bool is_opened()const { return context_ != nullptr; }
			AVCodecContext* context()const { return context_; }



			bool open();
			void close();



			bool input(const AVFrame* frame);
			bool flush();

		private:
			//bool send_frame(const AVFrame* inframe, AVFrame** encframe = nullptr);
			bool receive_packet(AVPacket* packet);

		public:
			sys::callback<audio_packet> on_packet;
		private:
			AVCodecContext* context_ = nullptr;
			audio_encoder_options options_;
			utils::audio_fifo fifo_;
			int frame_size_ = 0;
			int64_t pts_ = 0;
		};

		typedef std::shared_ptr<audio_encoder> audio_encoder_ptr;
	}
}

