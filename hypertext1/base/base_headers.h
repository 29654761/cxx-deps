#pragma once

#include <vector>
#include <string>

class base_headers
{
public:
	typedef struct item_t
	{
		std::string name;
		std::string value;
	}item_t;

	base_headers();
	~base_headers();

	void add(const std::string& name, const std::string& value);
	void set(const std::string& name, const std::string& value);
	bool get(const std::string& name, std::string& value)const;
	void get(const std::string& name, std::vector<std::string>& values)const;
	void remove(const std::string& name);
	void clear();

	std::vector<item_t>::const_iterator find(const std::string& name)const;
	std::vector<item_t>::iterator find_mutable(const std::string& name);

	static bool parse_content_length(const std::string& data,size_t offset,int64_t& length);
	static bool parse_content_length(const char* data, size_t size, int64_t& length);
public:
	std::vector<item_t> items;
};

