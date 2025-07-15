#pragma once
#ifndef _WIN32
#include <dlfcn.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <mutex>

class any_module
{
public:
	
	static any_module* get_instance();
	static void release();

	void* handle() {
		return module_;
	}
private:
	any_module();
	~any_module();

private:
	bool load();
	void free();
private:
	static std::recursive_mutex s_mutex;
	static any_module* s_instance;

#ifdef _WIN32
	HMODULE module_ = nullptr;
#else
	void* module_ = nullptr;
#endif
};

