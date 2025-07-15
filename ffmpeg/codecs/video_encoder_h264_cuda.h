#pragma once
#include "video_encoder.h"
#include <string>

namespace ffmpeg
{
	namespace codecs
	{
		class video_encoder_h264_cuda:public video_encoder
		{
		public:
			video_encoder_h264_cuda(const video_encoder_options& options);
			~video_encoder_h264_cuda();

			virtual bool open();
		private:
			std::string codec_name_;
			const AVCodecHWConfig* hwcfg_ = nullptr;
		};

	}
}
