#include "nal_splicer.h"
#include "h264.h"
#include "bit_reader.h"

nal_splicer::nal_splicer()
{
}

nal_splicer::~nal_splicer()
{
}

void nal_splicer::insert_nal(const uint8_t* nal, size_t size, std::vector<std::vector<uint8_t>>& frames)
{
	frames.clear();
	if (!nal || size <= 0) {
		auto frame = combin();
		if (frame.size() > 0) {
			frames.push_back(frame);
		}
		nals_.clear();
		return;
	}
	auto t=h264::get_nal_type(nal[0]);
	std::vector<uint8_t> new_nal;
	new_nal.reserve(4+size);
	new_nal.insert(new_nal.end(),3,0);
	new_nal.push_back(1);
	new_nal.insert(new_nal.end(), nal, nal + size);

	if (t != h264::nal_type_t::slice_non_idr && t != h264::nal_type_t::idr)
	{
		auto frame = combin();
		if (frame.size() > 0) {
			frames.push_back(frame);
		}
		nals_.clear();

		frames.push_back(new_nal);
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
			if (frame.size() > 0) {
				frames.push_back(frame);
			}
			nals_.clear();
		}

		nals_.push_back(new_nal);
	}
}


std::vector<uint8_t> nal_splicer::combin()
{
	std::vector<uint8_t> frame;
	for (auto& nal : nals_)
	{
		frame.insert(frame.end(), nal.begin(), nal.end());
	}
	return frame;
}
