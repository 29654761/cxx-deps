#pragma once


extern "C"
{
#include <libavformat/avformat.h>
}

#include <string>
#include <memory>
#include <atomic>
#include <mutex>


namespace ffmpeg
{
	namespace formats
	{
		class reader
		{
		public:
			reader();
			virtual ~reader();

			bool is_loop()const { return is_loop_; }
			void set_loop(bool loop) { is_loop_ = loop; }

			bool is_rtsp_over_tcp()const { return rtsp_over_tcp_; }
			void set_rtsp_over_tcp(bool tcp) { rtsp_over_tcp_ = tcp; }

			virtual bool open(const std::string& url,const std::string& fmt="",const AVDictionary* options=nullptr);
			/// <summary>
			/// fmt: v4l2,dshow
			/// vcodec:  ffmpeg -f dshow -list_options true -i video="Integrated Webcam_FHD"
			/// </summary>
			bool open_video_device(const std::string& device_name, const std::string& fmt, int width, int height, int framerate, const std::string& vcodec);

			virtual void close();
			virtual bool read_packet(AVPacket* pkt);


			bool is_connected()const { return is_connected_; }
			bool is_active()const { return active_; }

			AVStream* get_stream(unsigned int idx)const;
			AVStream* find_stream(AVMediaType mt)const;
			unsigned int stream_count()const;

			uint64_t bytes_rate() { return bytes_rate_.exchange(0); }

		private:
			static int s_interrupt_cb(void* ctx);
		protected:
			mutable std::recursive_mutex mutex_;
			bool is_loop_ = false;
			bool is_connected_ = false;
			bool active_ = false;
			AVFormatContext* format_ = nullptr;
			std::atomic<uint64_t> bytes_rate_;
			bool rtsp_over_tcp_ = false;
		};


		typedef std::shared_ptr<reader> reader_ptr;
	}
}