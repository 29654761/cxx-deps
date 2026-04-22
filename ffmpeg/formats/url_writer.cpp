#include "url_writer.h"
#include <thread>
#include <chrono>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
}

namespace ffmpeg
{
    namespace formats
    {

        url_writer::url_writer()
        {
            avio_flags = AVIO_FLAG_WRITE;
        }

        url_writer::~url_writer()
        {
        }



        bool url_writer::write_header()
        {
            std::lock_guard<std::recursive_mutex> lk(mutex_);
            if (!format_)
                return false;

            AVDictionary* opts = nullptr;

            if (rtsp_over_tcp_)
            {
                av_dict_set(&opts, "rtsp_transport", "tcp", 0);
                // prefer TCP interleaved
            }

            int r = avformat_write_header(format_, &opts);
            av_dict_free(&opts);
            if (r < 0)
            {
                char err[128] = { 0 };
                av_strerror(r, err, sizeof(err));
                return false;
            }

            return true;
        }

    }
}
