#pragma once

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <spdlog/spdlogger.hpp>

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

namespace ffmpeg
{
	namespace formats
	{
		class writer
		{
		public:
			writer();
			virtual ~writer();

			bool add_stream(const AVCodecParameters* pms);
			bool add_stream(const AVCodecContext* ctx);
			void clear_streams();
			unsigned int stream_count()const;
			AVStream* get_stream(unsigned int idx)const;
			AVStream* find_stream(AVMediaType mt)const;
			void set_logger(spdlogger_ptr log) { log_ = log; }
			bool is_active()const { return active_; }

			virtual bool open(const std::string& url, const std::string& fmt = "");
			virtual void close();
			virtual bool reopen();
			virtual bool write_header();
			virtual bool write_trailer();
			virtual bool write_packet(AVPacket* pkt);

			uint64_t bytes_rate() { return bytes_rate_.exchange(0); }

			static uint64_t read_duration(const std::string& file);
			static uint64_t read_size(const std::string& file);
		private:
			static int s_interrupt_cb(void* ctx);
		protected:
			spdlogger_ptr log_;
			mutable std::recursive_mutex mutex_;
			int avio_flags = 0;
			std::string url_;
			std::string fmt_;
			AVFormatContext* format_ = nullptr;
			std::vector<AVCodecParameters*> streams_par_;
			bool active_ = false;
			std::atomic<uint64_t> bytes_rate_;
			bool can_disconnect_ = true;
			bool has_error = false;
		};

	}
}

