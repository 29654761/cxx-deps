#include "reader.h"

namespace ffmpeg
{
	namespace formats
	{

		reader::reader()
		{
		}

		reader::~reader()
		{
		}

		bool reader::open(const std::string& url, const std::string& fmt, const AVDictionary* options)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (active_)
				return false;
			active_ = true;
			format_ = avformat_alloc_context();
			if (!format_)
			{
				return false;
			}

			format_->interrupt_callback.callback = s_interrupt_cb;
			format_->interrupt_callback.opaque = this;
			
			const AVInputFormat* infmt = nullptr;
			if (fmt.size() > 0) {
				infmt=av_find_input_format(fmt.c_str());
			}

			AVDictionary* opts = nullptr;
			if (options) {
				av_dict_copy(&opts, options, 0);
			}
			if (rtsp_over_tcp_) {
				av_dict_set(&opts, "rtsp_transport", "tcp", 0);
			}
			else {
				av_dict_set(&opts, "rtsp_transport", "udp", 0);
			}



			int r = avformat_open_input(&format_, url.c_str(), infmt, &opts);
			av_dict_free(&opts);
			if (r < 0)
			{
				char err[64] = {};
				av_make_error_string(err,64, r);
				close();
				return false;
			}

			

			r=avformat_find_stream_info(format_, nullptr);
			if (r < 0)
			{
				close();
				return false;
			}
			bytes_rate_ = 0;
			is_connected_ = true;
			return true;
		}

		bool reader::open_video_device(const std::string& device_name, const std::string& fmt, int width, int height, int framerate,const std::string& vcodec)
		{
			AVDictionary* options = nullptr;
			std::string fps = std::to_string(framerate);
			av_dict_set(&options, "framerate", fps.c_str(), 0);

			std::string video_size = std::to_string(width) + "x" + std::to_string(height);
			av_dict_set(&options, "video_size", video_size.c_str(), 0);

			av_dict_set(&options, "vcodec", vcodec.c_str(), 0);

			std::string url = "video=" + device_name;
			bool ret = open(url, fmt, options);

			av_dict_free(&options);
			return ret;
		}

		bool reader::open_audio_device(const std::string& device_name, const std::string& fmt, int samplerate, int channels,int milliseconds)
		{
			AVDictionary* options = nullptr;
			av_dict_set_int(&options, "sample_rate", samplerate, 0);
			av_dict_set_int(&options, "channels", channels, 0);
			av_dict_set_int(&options, "sample_size", 16, 0);  //bits per sample
			av_dict_set_int(&options, "audio_buffer_size", milliseconds, 0); //milliseconds
			//std::string bit_rate_str = std::to_string(bit_rate);
			//av_dict_set(&options, "bit_rate", bit_rate_str.c_str(), 0);

			std::string url = "audio=" + device_name;
			bool ret = open(url, fmt, options);

			av_dict_free(&options);
			return ret;
		}

		void reader::close()
		{
			active_ = false;
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (format_)
			{
				avformat_close_input(&format_);
			}
			is_connected_ = false;
		}

		AVStream* reader::get_stream(unsigned int idx)const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return nullptr;
			if (idx >= format_->nb_streams)
				return nullptr;
			return format_->streams[idx];
		}
		AVStream* reader::find_stream(AVMediaType mt)const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return nullptr;
			for (unsigned int i = 0; i < format_->nb_streams; i++)
			{
				if(format_->streams[i]->codecpar->codec_type==mt)
					return format_->streams[i];
			}
			return nullptr;
		}
		unsigned int reader::stream_count()const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return 0;
			return format_->nb_streams;
		}

		bool reader::read_packet(AVPacket* pkt)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return false;
			int r=av_read_frame(format_, pkt);
			if (r < 0)
			{
				is_connected_ = false;
				return false;
			}

			bytes_rate_ += pkt->size;
			return true;
		}

		int reader::s_interrupt_cb(void* ctx)
		{
			reader* p = (reader*)ctx;
			return p->active_ ? 0 : 1;
		}
	}
}

