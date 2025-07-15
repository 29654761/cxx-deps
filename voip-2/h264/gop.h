/**
 * @file gop.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include <string>
#include <vector>
#include <mutex>

class gop
{
public:

	struct frame_t
	{
		std::string frame;
		int type = 0;
		uint32_t duration = 0;
	};

	gop(int max_pkts=10000);
	~gop();

	bool add(const uint8_t* frame, uint32_t size,uint32_t duration);
	void clear();
	std::vector<frame_t> all();
private:
	int max_pkts_ = 10000;
	bool last_keyframe_ = true;
	std::recursive_mutex mutex_;
	std::vector<frame_t> frames_;

};
