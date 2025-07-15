#include "audio_fifo.h"

namespace ffmpeg
{
	namespace utils
	{

		audio_fifo::audio_fifo()
		{
		}

		audio_fifo::~audio_fifo()
		{
			close();
		}

		bool audio_fifo::is_opened()const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			return fifo_ != nullptr;
		}

		bool audio_fifo::open(AVSampleFormat format, int channels, int samples, bool grow)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (fifo_)
			{
				return false;
			}

			format_ = format;
			channels_ = channels;
			max_samples_ = samples;
			grow_ = grow;

			fifo_ = av_audio_fifo_alloc(format_, channels_, max_samples_);
			if (!fifo_)
			{
				return false;
			}
			return true;
		}

		void audio_fifo::close()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (fifo_)
			{
				av_audio_fifo_free(fifo_);
				fifo_ = nullptr;
			}
		}

		int audio_fifo::write(void** buffers, int samples)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!fifo_)
			{
				return -1;
			}

			int r = av_audio_fifo_write(fifo_, buffers, samples);

			if (!grow_)
			{
				if (av_audio_fifo_size(fifo_) >= max_samples_ + samples)
				{
					av_audio_fifo_drain(fifo_, samples);
				}
			}
			return r;
		}

		int audio_fifo::read(void** buffers, int samples)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!fifo_)
			{
				return -1;
			}

			return av_audio_fifo_read(fifo_, buffers, samples);
		}

		bool audio_fifo::read_samples(void** buffers, int samples)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!fifo_)
			{
				return false;
			}

			int fifo_samples = av_audio_fifo_size(fifo_);
			if (fifo_samples < samples)
			{
				return false;
			}

			int r = av_audio_fifo_read(fifo_, buffers, samples);
			if (r < 0)
			{
				return false;
			}
			return true;
		}

		int audio_fifo::samples()const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!fifo_)
			{
				return -1;
			}
			return av_audio_fifo_size(fifo_);
		}

	}
}

