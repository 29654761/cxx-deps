#pragma once

#include "video_decoder.h"
#include <string>

namespace ffmpeg
{
	namespace codecs
	{
		class video_decoder_qsv:public video_decoder
		{
		public:
			video_decoder_qsv(const video_decoder_options& options);
			~video_decoder_qsv();

			virtual bool open();
			virtual bool receive_frame(AVFrame* frame);

		private:
			std::string codec_name_;
			const AVCodecHWConfig* hwcfg_ = nullptr;
		};


	}
}