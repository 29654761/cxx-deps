#pragma once


extern "C"
{
#include <libavcodec/bsf.h>
#include <libavcodec/avcodec.h>
}

#include <string>


namespace ffmpeg
{
	namespace utils
	{

		class bit_stream_filter
		{
		public:
			bit_stream_filter();
			~bit_stream_filter();

			bool is_opened()const { return context_ != nullptr; }

			/// <summary>
			/// 
			/// </summary>
			/// <param name="codec"></param>
			/// <param name="bsf_name">h264_mp4toannexb,aac_adtstoasc</param>
			/// <returns></returns>
			bool open(const AVCodecContext* codec, const std::string& bsf_name);
			bool open(const AVCodecParameters* par, const std::string& bsf_name);

			void close();

			bool send_packet(AVPacket* packet);

			bool receive_packet(AVPacket* packet);
		private:
			AVBSFContext* context_ = nullptr;
		};

	}
}

