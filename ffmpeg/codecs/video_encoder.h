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
		struct video_encoder_options
		{
			AVCodecID codec_id = AVCodecID::AV_CODEC_ID_NONE;
			long bitrate = 0;
			long max_bitrate = 0;
			long min_bitrate = 0;
			AVPixelFormat format = AVPixelFormat::AV_PIX_FMT_NONE;
			int width = 0;
			int height = 0;
			int framerate = 0;
			int gop_size = 0;
			int thread_count = 1;
			int profile = FF_PROFILE_H264_BASELINE;
			int level = 40;  //10,11,12,13,20,21,30,31,32,40,41,42,50,51,52
			int refs = 2;

			video_encoder_options()
			{
			}

			video_encoder_options(int w, int h,int framerate)
			{
				this->width = w;
				this->height = h;
				this->framerate = framerate;
				this->gop_size = framerate * 4;

				int size = width * height;

				if (size <= 640 * 480)
				{
					bitrate = 512 * 1024;
					max_bitrate = 1024 * 1024;
					min_bitrate = 448 * 1024;
				}
				else if (size <= 1280 * 720)
				{
					bitrate = 1024 * 1024;
					max_bitrate = static_cast<long>(1024 * 1024*1.5);
					min_bitrate = 768 * 1024;
				}
				else
				{
					bitrate = 1024 * 1024 * 2;
					max_bitrate = 1024 * 1024 * 3;
					min_bitrate = static_cast<long>(1720 * 1024 * 1.5);
				}

			}
		};

		class video_encoder
		{
		public:
			typedef void (*video_packet)(void* ctx, AVPacket* packet);

			video_encoder(const video_encoder_options& options);
			virtual ~video_encoder();

			bool is_opened()const { return context_; }
			const AVCodecContext* context()const { return context_; }

			bool is_changed(AVPixelFormat format, int width, int height, int framerate)const
			{
				return (options_.format != format || options_.width != width || options_.height != height || options_.framerate != framerate);
			}

			virtual bool open() = 0;
			virtual void close();

			virtual bool input(const AVFrame* frame);
			virtual bool send_frame(const AVFrame* frame);
			virtual bool receive_packet(AVPacket* packet);
			virtual void required_keyframe();
		public:
			sys::callback<video_packet> on_packet;
		protected:
			AVCodecContext* context_ = nullptr;
			video_encoder_options options_;
			bool keyframe_ = false;
		};

		typedef std::shared_ptr<video_encoder> video_encoder_ptr;
	}
}

