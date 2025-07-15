#pragma once

#include "writer.h"
#include "../utils/bit_stream_filter.h"

namespace ffmpeg
{
	namespace formats
	{
		class hls_writer:public writer
		{
		public:
			hls_writer();
			~hls_writer();

			virtual bool open(const std::string& url, const std::string& fmt = "");
			virtual void close();
			virtual bool write_header();
			virtual bool write_packet(AVPacket* pkt);

			static uint64_t read_duration(const std::string& m3u8file);
			static uint64_t read_size(const std::string& m3u8file);
			static void repair_end_tag(const std::string& m3u8file);

		protected:
			ffmpeg::utils::bit_stream_filter filter_;
		};

	}
}

