/**
 * @file h265_gop.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include "h265.h"
#include <string>
#include <vector>
#include <mutex>

class h265_gop
{
public:

	struct frame_t
	{
		std::string frame;
		h265::nal_type_t type = h265::nal_type_t::reserved;
		uint32_t duration = 0;
	};

	h265_gop(size_t max_pkts=10000);
	~h265_gop();

	bool add(const uint8_t* frame, uint32_t size,uint32_t duration);
	void clear();
	std::vector<frame_t> all();
private:
	size_t max_pkts_ = 10000;
	bool last_keyframe_ = true;
	std::recursive_mutex mutex_;
	std::vector<frame_t> frames_;

};
