#pragma once

#include "writer.h"
#include <thread>
#include <atomic>
#include <sys2/blocking_queue.hpp>

namespace ffmpeg
{
    namespace formats
    {
        class url_writer : public writer
        {
        public:
            url_writer();
            ~url_writer();

            // No explicit invocation is needed
            virtual bool write_header() override;
			void set_rtsp_over_tcp(bool rtsp_over_tcp) { rtsp_over_tcp_ = rtsp_over_tcp; }
        private:
            bool rtsp_over_tcp_ = false;

        };

    }
}
