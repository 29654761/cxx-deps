#pragma once
#include "video_encoder.h"

namespace ffmpeg
{
	namespace codecs
	{
		class video_encoder_h265_soft :public video_encoder
		{
		public:
			video_encoder_h265_soft(const video_encoder_options& options);
			~video_encoder_h265_soft();

			virtual bool open();
		private:

		};

	}
}
