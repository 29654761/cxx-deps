#include "writer.h"
#include <thread>
#include <sys2/util.h>
#include <sys2/fs.h>

namespace ffmpeg
{
	namespace formats
	{

		writer::writer()
			:bytes_rate_(0)
		{
			
		}

		writer::~writer()
		{

			clear_streams();
		}

		bool writer::add_stream(const AVCodecParameters* pms)
		{
			
			std::lock_guard<std::recursive_mutex> lk(mutex_);

			AVCodecParameters* pms2 = avcodec_parameters_alloc();
			if (!pms2)
				return false;
			int r=avcodec_parameters_copy(pms2, pms);
			if (r < 0)
			{
				avcodec_parameters_free(&pms2);
				return false;
			}
			pms2->codec_tag = 0;
			streams_par_.push_back(pms2);
			return true;
		}

		bool writer::add_stream(const AVCodecContext* ctx)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);

			AVCodecParameters* pms2 = avcodec_parameters_alloc();
			if (!pms2)
				return false;
			int r = avcodec_parameters_from_context(pms2, ctx);
			if (r < 0)
			{
				avcodec_parameters_free(&pms2);
				return false;
			}
			pms2->codec_tag = 0;
			streams_par_.push_back(pms2);
			return true;
		}

		void writer::clear_streams()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			for (auto itr = streams_par_.begin(); itr != streams_par_.end(); itr++)
			{
				AVCodecParameters* pms = *itr;
				avcodec_parameters_free(&pms);
			}
		}

		AVStream* writer::get_stream(unsigned int idx)const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return nullptr;
			if (idx >= format_->nb_streams)
				return nullptr;
			return format_->streams[idx];
		}
		AVStream* writer::find_stream(AVMediaType mt)const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return nullptr;
			
			for (unsigned int i = 0; i < format_->nb_streams; i++)
			{
				if (format_->streams[i]->codecpar->codec_type == mt)
					return format_->streams[i];
			}
			return nullptr;
		}


		unsigned int writer::stream_count()const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return 0;
			return format_->nb_streams;
		}

		bool writer::open(const std::string& url, const std::string& fmt)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (active_)
				return true;
			active_ = true;
			url_ = url;
			fmt_ = fmt;
			const char* sfmt = nullptr;
			if (fmt_.size() > 0)
				sfmt = fmt_.c_str();


			int r = avformat_alloc_output_context2(&format_, nullptr, sfmt, url_.c_str());
			if (r < 0)
			{
				close();
				return false;
			}
			format_->interrupt_callback.callback = s_interrupt_cb;
			format_->interrupt_callback.opaque = this;
			
			for (auto itr = streams_par_.begin(); itr != streams_par_.end(); itr++)
			{
				AVCodecParameters* pms = *itr;
				const AVCodec* c = avcodec_find_encoder(pms->codec_id);
				if (c)
				{
					AVStream* stream=avformat_new_stream(format_, c);
					if (stream)
					{
						avcodec_parameters_copy(stream->codecpar, pms);
					}
				}
			}


			r = avio_open2(&format_->pb, url_.c_str(), avio_flags| AVIO_FLAG_READ_WRITE, nullptr, nullptr);
			if (r < 0)
			{
				close();
				return false;
			}
			bytes_rate_ = 0;
			return true;
		}

		void writer::close()
		{
			active_ = false;
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (format_)
			{
				if (format_->pb)
				{
					avio_closep(&format_->pb);
				}

				avformat_free_context(format_);
				format_ = nullptr;
			}

		}

		bool writer::reopen()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);

			if (format_)
			{
				if (format_->pb)
				{
					avio_closep(&format_->pb);
				}

				avformat_free_context(format_);
				format_ = nullptr;
			}

			const char* sfmt = nullptr;
			if (fmt_.size() > 0)
				sfmt = fmt_.c_str();

			int r = avformat_alloc_output_context2(&format_, nullptr, sfmt, url_.c_str());
			if (r < 0)
			{
				if (log_)
				{
					log_->error("avformat_alloc_output_context2 failed, url={}",url_)->flush();
				}
				return false;
			}

			for (auto itr = streams_par_.begin(); itr != streams_par_.end(); itr++)
			{
				AVCodecParameters* pms = *itr;
				const AVCodec* c = avcodec_find_encoder(pms->codec_id);
				if (c)
				{
					AVStream* stream = avformat_new_stream(format_, c);
					if (stream)
					{
						avcodec_parameters_copy(stream->codecpar, pms);
						stream->duration = 0;
					}
				}
			}

			r = avio_open2(&format_->pb, url_.c_str(), avio_flags| AVIO_FLAG_READ_WRITE, nullptr, nullptr);
			if (r < 0)
			{
				if (log_)
				{
					log_->error("avio_open2 failed, url={}", url_)->flush();
				}
				avformat_free_context(format_);
				return false;
			}

			return true;
		}

//avformat_alloc_output_context2(&oc, NULL, "segment", "output_%03d.mp4");
//oformat = av_guess_format("segment", NULL, NULL);
//av_opt_set(oc->priv_data, "segment_time", "10", 0);
//av_opt_set(oc->priv_data, "segment_format", "mp4", 0);
//av_opt_set(oc->priv_data, "reset_timestamps", "1", 0);

		bool writer::write_header()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return false;

			const char* fmt = format_->oformat->name;

			AVDictionary* opts = nullptr;

			if (strcmp(fmt, "mp4") == 0 ||
				strcmp(fmt, "mov") == 0)
			{
				AVStream* vs = find_stream(AVMEDIA_TYPE_VIDEO);
				if (vs)
				{
					av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
				}
				else
				{
					av_dict_set(&opts, "movflags", "empty_moov", 0);
					av_dict_set(&opts, "frag_duration", "1000000", 0); //1s
					//av_dict_set(&opts, "frag_size", "50000", 0);  //
				}
			}

			int r = avformat_write_header(format_, &opts);
			av_dict_free(&opts);
			if (r < 0)
			{
				char err[64] = {};
				av_make_error_string(err, 64, r);

				if (log_)
				{
					log_->error("avformat_write_header failed,err={}, url={}",err, url_)->flush();
				}
				return false;
			}
			return true;
		}

		bool writer::write_trailer()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return false;
			int r = av_write_trailer(format_);
			return r >= 0;
		}

		bool writer::write_packet(AVPacket* pkt)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return false;

			int r = av_write_frame(format_, pkt);
			if (r < 0) {
				return false;
			}

			bytes_rate_ += pkt->size;
			return true;
		}

		uint64_t writer::read_duration(const std::string& file)
		{
			AVFormatContext* fmt = nullptr;
			
			int r = avformat_open_input(&fmt, file.c_str(), nullptr, nullptr);
			if (r < 0) {
				return 0;
			}

			r = avformat_find_stream_info(fmt, nullptr);
			if (r < 0) {
				avformat_close_input(&fmt);
				return 0;
			}

			uint64_t duration = fmt->duration / 1000;
			avformat_close_input(&fmt);
			return duration;
		}

		uint64_t writer::read_size(const std::string& file)
		{
			std::error_code ec;
			uint64_t r = fs::file_size(file,ec);
			if (ec)
				return 0;
			else
				return r;
		}

		int writer::s_interrupt_cb(void* ctx)
		{
			writer* p = (writer*)ctx;
			return p->active_ ? 0 : 1;
		}
	}
}

