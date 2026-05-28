#include "fs.h"

namespace sys
{

	fs::path fs_relative(fs::path p, fs::path base)
	{
		std::error_code ec;
		// 1. convert p and base to absolute paths
		p = fs::absolute(p,ec);
		base = fs::absolute(base,ec);

		// 2. find first mismatch and shared root path
		auto mismatched = std::mismatch(p.begin(), p.end(), base.begin(), base.end());

		// 3. if no mismatch return "."
		if (mismatched.first == p.end() && mismatched.second == base.end())
			return ".";

		auto it_p = mismatched.first;
		auto it_base = mismatched.second;

		fs::path ret;

		// 4. iterate abase to the shared root and append "../"
		for (; it_base != base.end(); ++it_base)
		{
			if (!it_base->empty()) {
				ret /= "..";
			}
		}
		// 5. iterate from the shared root to the p and append its parts
		for (; it_p != p.end(); ++it_p)
		{
			ret /= *it_p;
		}
		return ret;
	}



	void remove_empty_directories(const fs::path& path, std::error_code& ec, bool remove_root)
	{
		if (!fs::exists(path, ec) || !fs::is_directory(path, ec))
			return;

		for (const auto& entry : fs::directory_iterator(path))
		{
			if (fs::is_directory(entry.path(), ec))
			{
				remove_empty_directories(entry.path(), ec, true);
			}
		}

		if (remove_root)
		{
			if (fs::is_empty(path, ec))
			{
				fs::remove(path, ec);
			}
		}
	}
}

