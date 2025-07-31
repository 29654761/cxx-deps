#pragma once
#include <vector>
#include <functional>

class nal_splicer
{
public:

	struct nal_item_t
	{
		std::vector<uint8_t> nal;
		uint32_t first_mbs = 0;
	};

	nal_splicer();
	~nal_splicer();

	void insert_nal(const uint8_t* nal, size_t size, std::vector<std::vector<uint8_t>>& frames);

	void clear() { nals_.clear(); }
private:
	std::vector<uint8_t> combin();
private:
	std::vector<nal_item_t> nals_;
};

