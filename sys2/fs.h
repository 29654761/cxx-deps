#pragma once

#ifdef _WIN32
#include <yvals_core.h>
#endif

#ifdef _STD_FS_EX
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace sys
{
	fs::path fs_relative(fs::path p, fs::path base);
}

