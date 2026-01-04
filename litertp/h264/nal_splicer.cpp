#include "nal_splicer.h"
#include "h264.h"
#include "bit_reader.h"
#include <algorithm>

nal_splicer::nal_splicer()
{
}

nal_splicer::~nal_splicer()
{
}

void nal_splicer::insert_nal(const uint8_t* nal, size_t size, int64_t pts, int64_t dts, std::vector<nal_item_t>& frames)
{
	frames.clear();
	if (!nal || size <= 0) {
		auto frame = combin();
		if (frame.nal.size() > 0) {
			frames.push_back(frame);
		}
		nals_.clear();
		return;
	}
	auto t=h264::get_nal_type(nal[0]);
	nal_item_t new_item;
	new_item.pts = pts;
	new_item.dts = dts;
	new_item.first_mbs = 0;
	new_item.nal.reserve(4+size);
	new_item.nal.insert(new_item.nal.end(),3,0);
	new_item.nal.push_back(1);
	new_item.nal.insert(new_item.nal.end(), nal, nal + size);

	if (t != h264::nal_type_t::slice_non_idr && t != h264::nal_type_t::idr)
	{
		auto frame = combin();
		if (frame.nal.size() > 0) {
			frames.push_back(frame);
		}
		nals_.clear();

		frames.push_back(new_item);
	}
	else
	{
		bit_reader bit(nal+1, size);
		uint32_t first_mbs = 0;
		if (!bit.read_ue(first_mbs))
			return;

		if (first_mbs == 0)
		{
			auto frame = combin();
			if (frame.nal.size() > 0) {
				frames.push_back(frame);
			}
			nals_.clear();
		}

		new_item.first_mbs = first_mbs;
		nals_.push_back(new_item);
	}
}


nal_splicer::nal_item_t nal_splicer::combin()
{
	std::sort(nals_.begin(), nals_.end(), [](const nal_item_t& a, const nal_item_t& b) {
		return a.first_mbs < b.first_mbs;
	});

	nal_item_t frame;
	frame.first_mbs = 0;
	for (auto itr=nals_.begin();itr!=nals_.end();itr++)
	{
		if(itr==nals_.begin())
		{
			frame.pts = itr->pts;
			frame.dts = itr->dts;
		}
		frame.nal.insert(frame.nal.end(), itr->nal.begin(),itr->nal.end());
	}
	return frame;
}
