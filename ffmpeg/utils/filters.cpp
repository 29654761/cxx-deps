#include "filters.h"

namespace ffmpeg
{
	namespace utils
	{

		filters::filters()
		{

		}

		filters::~filters()
		{
			close();
		}

		bool filters::open()
		{
			if (filter_graph_)
			{
				return false;
			}



			filter_graph_ = avfilter_graph_alloc();
			if (!filter_graph_)
			{
				return false;
			}



			return true;
		}


		void filters::close()
		{
			if (filter_graph_ != NULL)
			{
				avfilter_graph_free(&filter_graph_);
			}
		}

		bool filters::is_active()const
		{
			return filter_graph_ && in_filter_ && out_filter_;
		}

		bool filters::is_changed(int width, int height, AVPixelFormat format)const
		{
			return width_ != width || height_ != height || format_ != format;
		}

		bool filters::is_changed(const AVFrame* frame)const
		{
			return is_changed(frame->width, frame->height, (AVPixelFormat)frame->format);
		}

		bool filters::add_in_buffer(int width, int height, AVPixelFormat format)
		{
			char args[512] = { 0 };
			snprintf(args, 511, "video_size=%dx%d:pix_fmt=%d:time_base=1/20:pixel_aspect=1/1", width, height, (int)format);

			const AVFilter* buffer = avfilter_get_by_name("buffer");
			int r = avfilter_graph_create_filter(&in_filter_, buffer, "in", args, NULL, filter_graph_);
			if (r < 0)
			{
				return false;
			}
			pre_filter_ = in_filter_;
			width_ = width;
			height_ = height;
			format_ = format;
			return true;
		}

		bool filters::add_out_buffer()
		{
			const AVFilter* buffersink = avfilter_get_by_name("buffersink");
			int r = avfilter_graph_create_filter(&out_filter_, buffersink, "out", NULL, NULL, filter_graph_);
			if (r < 0)
			{
				return false;
			}

			r = avfilter_link(pre_filter_, 0, out_filter_, 0);
			if (r < 0)
			{
				avfilter_free(out_filter_);
				return false;
			}

			pre_filter_ = out_filter_;

			r = avfilter_graph_config(filter_graph_, nullptr);
			if (r < 0)
			{
				avfilter_free(out_filter_);
				return false;
			}

			return true;
		}


		bool filters::add_hflip()
		{
			AVFilterContext* filter = nullptr;
			const AVFilter* hflip = avfilter_get_by_name("hflip");
			int r = avfilter_graph_create_filter(&filter, hflip, "hflip", nullptr, nullptr, filter_graph_);
			if (r < 0)
			{
				return false;
			}

			r = avfilter_link(pre_filter_, 0, filter, 0);
			if (r < 0)
			{
				avfilter_free(filter);
				return false;
			}

			pre_filter_ = filter;
			return true;
		}

		bool filters::add_vflip()
		{
			AVFilterContext* filter = nullptr;
			const AVFilter* vflip = avfilter_get_by_name("vflip");
			int r = avfilter_graph_create_filter(&filter, vflip, "vflip", nullptr, nullptr, filter_graph_);
			if (r < 0)
			{
				return false;
			}

			r = avfilter_link(pre_filter_, 0, filter, 0);
			if (r < 0)
			{
				avfilter_free(filter);
				return false;
			}

			pre_filter_ = filter;
			return true;
		}

		bool filters::add_overlay()
		{
			char args[512] = { 0 };
			//snprintf(args, 511, "x=%d:y=%d:repeatlast=1"); //top left
			//snprintf(args, 511, "x=W-w-%d:y=%d:repeatlast=1"); //top right
			//snprintf(args, 511, "x=%d:y=H-h-%d:repeatlast=1""); //bottom left
			//snprintf(args, 511, "x=W-w-%d:y=H-h-%d:repeatlast=1"); // bottom right
			//snprintf(args, 511, "x=(W-w)/2:y=(H-h)/2:repeatlast=1"); // center

			AVFilterContext* filter = nullptr;
			const AVFilter* vflip = avfilter_get_by_name("overlay");
			int r = avfilter_graph_create_filter(&filter, vflip, "overlay", args, nullptr, filter_graph_);
			if (r < 0)
			{
				return false;
			}

			r = avfilter_link(pre_filter_, 0, filter, 0);
			if (r < 0)
			{
				avfilter_free(filter);
				return false;
			}

			pre_filter_ = filter;
			return true;
		}

		bool filters::add_transpose(int dir, const char* passthrough)
		{
			char args[512] = { 0 };
			snprintf(args, 511, "dir=%d:passthrough=%s", dir, passthrough);

			AVFilterContext* filter = nullptr;
			const AVFilter* vflip = avfilter_get_by_name("transpose");
			int r = avfilter_graph_create_filter(&filter, vflip, "transpose", args, nullptr, filter_graph_);
			if (r < 0)
			{
				return false;
			}

			r = avfilter_link(pre_filter_, 0, filter, 0);
			if (r < 0)
			{
				avfilter_free(filter);
				return false;
			}

			pre_filter_ = filter;
			return true;
		}


		bool filters::add_frame(AVFrame* frame)
		{
			int r = av_buffersrc_add_frame(in_filter_, frame);
			return r >= 0 ? true : false;
		}

		bool filters::get_frame(AVFrame* frame)
		{
			int r = av_buffersink_get_frame(out_filter_, frame);
			return r >= 0 ? true : false;
		}

	}
}

