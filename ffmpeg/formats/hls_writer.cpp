#include "hls_writer.h"
#include <fstream>
#include <sys2/string_util.h>
#include <sys2/fs.h>


namespace ffmpeg
{
	namespace formats
	{

		hls_writer::hls_writer()
		{
			//avio_flags = AVIO_FLAG_DIRECT;
			can_disconnect_ = false;
		}

		hls_writer::~hls_writer()
		{
		}

		bool hls_writer::open(const std::string& url, const std::string& fmt)
		{
			if (!writer::open(url,fmt))
			{
				return false;
			}
			AVStream* stream=this->find_stream(AVMediaType::AVMEDIA_TYPE_VIDEO);
			if (stream)
			{
				filter_.open(stream->codecpar,"h264_mp4toannexb");
			}
			return true;
		}

		void hls_writer::close()
		{
			writer::close();
			filter_.close();
		}

		bool hls_writer::write_header()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!format_)
				return false;

			AVDictionary* opts = nullptr;
			av_dict_set_int(&opts, "hls_init_time", 0, 0);
			av_dict_set_int(&opts, "hls_list_size", 0, 0);
			av_dict_set_int(&opts, "hls_time", 5, 0);
			av_dict_set(&opts, "hls_flags", "+temp_file+append_list", 0);
			int r = avformat_write_header(format_, &opts);
			av_dict_free(&opts);
			if (r < 0)
			{
				return false;
			}

			if (format_->pb)
			{
				avio_close(format_->pb);
				format_->pb = nullptr;
			}

			return r >= 0 ? true : false;
		}

		bool hls_writer::write_packet(AVPacket* pkt)
		{
			AVStream* avs = this->get_stream(pkt->stream_index);
			if (avs == nullptr)
				return false;

			if (avs->codecpar->codec_type==AVMediaType::AVMEDIA_TYPE_VIDEO&&filter_.is_opened())
			{
				AVPacket* f_in_pkt = av_packet_clone(pkt);
				if (!f_in_pkt)
					return false;

				if (!filter_.send_packet(f_in_pkt))
				{
					av_packet_free(&f_in_pkt);
					return false;
				}
				AVPacket f_out_pkt = {};
				if (!filter_.receive_packet(&f_out_pkt))
				{
					av_packet_free(&f_in_pkt);
					return false;
				}

				bool r=writer::write_packet(&f_out_pkt);
				av_packet_free(&f_in_pkt);
				av_packet_unref(&f_out_pkt);
				return r;
			}
			else
			{
				return writer::write_packet(pkt);
			}
		}

		uint64_t hls_writer::read_duration(const std::string& m3u8file)
		{
			uint64_t duration = 0;
			std::ifstream file(m3u8file);
			if (!file.is_open())
			{
				return duration;
			}

			std::string line;
			char* endptr = nullptr;
			while (std::getline(file, line)) 
			{
				sys::string_util::trim(line);
				if (line.empty())
					continue;
				if (sys::string_util::is_start_of(line,"#EXTINF:"))
				{
					std::string sdur = line.substr(8);
					double dur=strtod(sdur.c_str(), &endptr);
					duration += (int64_t)(dur * 1000000);
				}

			}

			return duration;

		}

		uint64_t hls_writer::read_size(const std::string& m3u8file)
		{
			uint64_t filesize = 0;
			std::ifstream file(m3u8file);
			if (!file.is_open())
			{
				return filesize;
			}

			filesize += fs::file_size(m3u8file);
			fs::path path(m3u8file);
			std::string dir = path.parent_path().string();

			std::string line;
			char* endptr = nullptr;
			while (std::getline(file, line))
			{
				sys::string_util::trim(line);
				if (line.empty())
					continue;
				if (!sys::string_util::is_start_of(line, "#"))
				{
					std::string path_file = sys::string_util::join_path(dir, line, '/');
					filesize += fs::file_size(path_file);
				}

			}

			return filesize;
		}

		void hls_writer::repair_end_tag(const std::string& m3u8file)
		{
			double duration = 0.0;
			std::ifstream file(m3u8file);
			if (!file.is_open())
			{
				return;
			}

			std::vector<std::string> lines;
			std::string line;
			while (std::getline(file, line))
			{
				sys::string_util::trim(line);
				if (line.empty())
					continue;

				lines.push_back(line);

			}

			if (lines.size() > 0 && *(lines.end()-1) != "#EXT-X-ENDLIST")
			{
				lines.push_back("#EXT-X-ENDLIST");
			}
			else
			{
				return;
			}
			file.close();

			std::ofstream ofile(m3u8file);
			for (auto itr = lines.begin(); itr != lines.end(); itr++)
			{
				ofile << *itr << std::endl;
			}

			ofile.close();
		}
	}
}

