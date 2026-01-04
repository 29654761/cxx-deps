#pragma once
#include <vector>
#include <functional>
#include <cstdint>

class nal_splicer
{
public:

	struct nal_item_t
	{
		std::vector<uint8_t> nal;
		uint32_t first_mbs = 0;
		int64_t pts = 0;
		int64_t dts = 0;
	};

	nal_splicer();
	~nal_splicer();

	void insert_nal(const uint8_t* nal, size_t size,int64_t pts,int64_t dts, std::vector<nal_item_t>& frames);

	void clear() { nals_.clear(); }
private:
	nal_item_t combin();
private:
	std::vector<nal_item_t> nals_;
};

