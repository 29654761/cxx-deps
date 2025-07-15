#pragma once
#include "video_encoder.h"

namespace ffmpeg
{
	namespace codecs
	{
		class video_encoder_vp8_soft:video_encoder
		{
		public:
			video_encoder_vp8_soft(const video_encoder_options& options);
			~video_encoder_vp8_soft();

			virtual bool open();
		private:

		};

	}
}
