#include "any_module.h"

any_module::any_module()
{
}

any_module::~any_module()
{
}


std::recursive_mutex any_module::s_mutex;
any_module* any_module::s_instance = nullptr;

any_module* any_module::get_instance()
{
	std::lock_guard<std::recursive_mutex> lk(any_module::s_mutex);
	if (s_instance)
		return s_instance;

	s_instance = new any_module();
	if (!s_instance->load())
	{
		delete s_instance;
		s_instance = nullptr;
		return nullptr;
	}
	return s_instance;
}

void any_module::release()
{
	std::lock_guard<std::recursive_mutex> lk(any_module::s_mutex);
	if (s_instance)
	{
		s_instance->free();
		delete s_instance;
		s_instance = nullptr;
	}
}

bool any_module::load()
{
#ifdef _WIN32
	module_ = LoadLibraryA("linkmic_service.dll");
	if (!module_) {
		return false;
	}

#else
	module_ = dlopen("liblinkmic_service.so", RTLD_LAZY);
	if (!module_) {
		printf("Load liblinkmic_service.so error: %s\n", dlerror());
		return false;
	}
#endif
	return true;
}

void any_module::free()
{
	if (module_) {
#ifdef _WIN32
		FreeLibrary(module_);
#else
		dlclose(module_);
#endif
		module_ = nullptr;
	}
}
