#include "mirror_filter.h"


namespace ffmpeg
{
	namespace utils
	{

		mirror_filter::mirror_filter()
		{

		}

		mirror_filter::~mirror_filter()
		{
			close();
		}

		bool mirror_filter::open(int width, int height, AVPixelFormat format, direction_t dir)
		{
			if (filter_graph_)
			{
				close();
				return false;
			}

			width_ = width;
			height_ = height;
			format_ = format;

			filter_graph_ = avfilter_graph_alloc();
			if (!filter_graph_)
			{
				close();
				return false;
			}

			char args[512] = { 0 };
			snprintf(args, 511, "video_size=%dx%d:pix_fmt=%d:time_base=1/20:pixel_aspect=1/1", width, height, (int)format);

			const AVFilter* buffer = avfilter_get_by_name("buffer");
			int r = avfilter_graph_create_filter(&filter_in_, buffer, "in", args, NULL, filter_graph_);
			if (r < 0)
			{
				close();
				return false;
			}

			AVFilterContext* flip_ctx = nullptr;
			if (dir == direction_t::hflip)
			{
				const AVFilter* hflip = avfilter_get_by_name("hflip");
				r = avfilter_graph_create_filter(&flip_ctx, hflip, "hflip", NULL, NULL, filter_graph_);
				if (r < 0)
				{
					close();
					return false;
				}
			}
			else if (dir == direction_t::vflip)
			{
				const AVFilter* vflip = avfilter_get_by_name("vflip");
				r = avfilter_graph_create_filter(&flip_ctx, vflip, "vflip", NULL, NULL, filter_graph_);
				if (r < 0)
				{
					close();
					return false;
				}
			}
			else
			{
				close();
				return false;
			}

			const AVFilter* buffersink = avfilter_get_by_name("buffersink");
			r = avfilter_graph_create_filter(&filter_out_, buffersink, "out", NULL, NULL, filter_graph_);
			if (r < 0)
			{
				close();
				return false;
			}

			r = avfilter_link(filter_in_, 0, flip_ctx, 0);
			if (r < 0)
			{
				close();
				return false;
			}

			r = avfilter_link(flip_ctx, 0, filter_out_, 0);
			if (r < 0)
			{
				close();
				return false;
			}

			r = avfilter_graph_config(filter_graph_, NULL);
			if (r < 0)
			{
				close();
				return false;
			}

			return true;
		}

		void mirror_filter::close()
		{
			if (filter_graph_ != NULL)
			{
				avfilter_graph_free(&filter_graph_);
			}
		}

		bool mirror_filter::is_opened()
		{
			return filter_graph_ ? true : false;
		}

		bool mirror_filter::is_changed(const AVFrame* frame)
		{
			if (frame->width != width_ || frame->height != height_ || frame->format != (int)format_)
			{
				return true;
			}
			return false;
		}

		void mirror_filter::ensure_created(const AVFrame* frame, direction_t dir)
		{
			if (is_changed(frame))
			{
				close();
			}
			if (!is_opened())
			{
				open(frame->width, frame->height, (AVPixelFormat)frame->format, dir);
			}
		}

		bool mirror_filter::add_frame(AVFrame* frame)
		{
			int r = av_buffersrc_add_frame(filter_in_, frame);
			return r >= 0 ? true : false;
		}

		bool mirror_filter::get_frame(AVFrame* frame)
		{
			int r = av_buffersink_get_frame(filter_out_, frame);
			return r >= 0 ? true : false;
		}

	}
}

