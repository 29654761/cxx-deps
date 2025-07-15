#include "headers.h"
#include <sys2/string_util.h>

namespace hypertext
{

	headers::headers()
	{
	}

	headers::~headers()
	{
	}


	void headers::add(const std::string& name, const std::string& value)
	{
		item_t item;
		item.name = name;
		item.value = value;
		items.push_back(item);
	}

	void headers::set(const std::string& name, const std::string& value)
	{
		std::vector<item_t>::iterator itr = find_mutable(name);
		if (itr == items.end())
		{
			add(name, value);
		}
		else
		{
			itr->value = value;
		}
	}

	bool headers::get(const std::string& name, std::string& value)const
	{
		auto itr = find(name);
		if (itr == items.end())
		{
			return false;
		}
		value = itr->value;
		return true;
	}

	void headers::get(const std::string& name, std::vector<std::string>& values)const
	{
		values.clear();
		for (auto itr = items.begin(); itr != items.end(); itr++)
		{
			if (sys::string_util::icasecompare(name, itr->name))
			{
				values.push_back(itr->value);
			}
		}
	}

	void headers::remove(const std::string& name)
	{
		for (auto itr = items.begin(); itr != items.end();)
		{
			if (sys::string_util::icasecompare(name, itr->name))
			{
				itr = items.erase(itr);
			}
			else
			{
				itr++;
			}
		}
	}

	void headers::clear()
	{
		items.clear();
	}


	std::vector<headers::item_t>::const_iterator headers::find(const std::string& name)const
	{
		return std::find_if(items.begin(), items.end(), [&name](const item_t& a) {
			return sys::string_util::icasecompare(a.name, name);
			});
	}

	std::vector<headers::item_t>::iterator headers::find_mutable(const std::string& name)
	{
		return std::find_if(items.begin(), items.end(), [&name](const item_t& a) {
			return sys::string_util::icasecompare(a.name, name);
			});
	}

	bool headers::parse_content_length(const std::string& data, size_t offset, int64_t& length)
	{
		size_t pos = data.find("Content-Length:", offset);
		if (pos == std::string::npos)
		{
			length = 0;
			return false;
		}
		char* endptr = nullptr;
		const char* p = data.c_str() + pos + 15;
		length = strtoll(p, &endptr, 10);
		return true;
	}

	bool headers::parse_content_length(const char* data, size_t size, int64_t& length)
	{
		const char* pos = sys::string_util::find_str(data, size, "Content-Length:", 15);
		if (!pos)
		{
			length = 0;
			return false;
		}
		char* endptr = nullptr;
		const char* p = pos + 15;
		length = strtoll(p, &endptr, 10);
		return true;
	}

}

