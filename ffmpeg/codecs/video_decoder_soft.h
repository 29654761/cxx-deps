#pragma once

#include "video_decoder.h"

namespace ffmpeg
{
	namespace codecs
	{
		class video_decoder_soft:public video_decoder
		{
		public:
			video_decoder_soft(const video_decoder_options& options);
			~video_decoder_soft();

			virtual bool open();
			virtual void close();
			virtual bool send_packet(const AVPacket* packet);
			virtual bool receive_frame(AVFrame* frame);

		};


	}
}