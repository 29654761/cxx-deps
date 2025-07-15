#pragma once

extern "C"
{
#include <libavutil/audio_fifo.h>
}
#include <mutex>

namespace ffmpeg
{
	namespace utils
	{
		class audio_fifo
		{
		public:
			audio_fifo();
			~audio_fifo();

			bool is_opened()const;

			bool open(AVSampleFormat format, int channels, int samples, bool grow = false);
			void close();

			int write(void** buffers, int samples);
			int read(void** buffers, int samples);
			bool read_samples(void** buffers, int samples);
			int samples()const;
		private:
			mutable std::recursive_mutex mutex_;
			AVAudioFifo* fifo_ = nullptr;
			AVSampleFormat format_ = AVSampleFormat::AV_SAMPLE_FMT_NONE;
			int channels_ = 0;
			int max_samples_ = 0;
			bool grow_ = false;
		};

	}
}
