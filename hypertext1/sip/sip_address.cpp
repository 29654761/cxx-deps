#include "sip_address.h"
#include <sys2/string_util.h>
#include <sstream>

sip_address::sip_address()
{
}

sip_address::sip_address(const std::string& display, const voip_uri& url)
{
	this->display = display;
	this->url = url;
}

sip_address::~sip_address()
{
}

void sip_address::parse(const std::string& s)
{
	display = "";
	int i = 0;
	bool has_display = false;
	for (; i < s.length(); i++)
	{
		char c = s.at(i);
		if (c == ' ' || c == '<')
		{
			has_display = true;
			break;
		}
		if (c != '\"') {
			display.append(1, c);
		}
	}
	if (!has_display)
	{
		display = "";
		url.parse(s);
		queries.clear();
		for (auto itr = url.queries.begin(); itr != url.queries.end(); itr++) 
		{
			query_item_t item;
			item.name = itr->name;
			item.value = itr->value;
			queries.push_back(item);
		}
		url.queries.clear();
	}
	else
	{
		bool has_angle_brackets = false;
		std::string suri;
		for (; i < s.length(); i++)
		{
			char c = s.at(i);
			if (c == ' ' || c == '<')
			{
				if (c == '<') {
					has_angle_brackets = true;
				}
				continue;
			}
			else if (c == '>')
			{
				break;
			}
			suri.append(1, c);
		}

		this->url.parse(suri);

		if (has_angle_brackets)
		{
			i++;
			if (i >= s.length())
			{
				return;
			}

			queries.clear();
			auto ss = sys::string_util::split(s.substr(i), ";");
			for (auto itr = ss.begin(); itr != ss.end(); itr++)
			{
				query_item_t q;
				auto ss2 = sys::string_util::split(*itr, "=", 2);
				if (ss2.size() >= 1) {
					q.name = ss2[0];
				}
				if (ss2.size() >= 2)
				{
					q.value = ss2[1];
				}
				queries.push_back(q);
			}
		}
		else
		{
			queries.clear();
			for (auto itr = url.queries.begin(); itr != url.queries.end(); itr++)
			{
				query_item_t item;
				item.name = itr->name;
				item.value = itr->value;
				queries.push_back(item);
			}
			url.queries.clear();
		}
	}
}

std::string sip_address::to_string()const
{
	std::stringstream ss;
	if (display.size() > 0) {
		ss << "\"" << display << "\" ";
	}
	ss << "<" << url.to_string() << ">";
	for(auto itr= queries.begin();itr!= queries.end();itr++)
	{
		ss << ";" << itr->name << "=" << itr->value;
	}
	return ss.str();
}


void sip_address::add(const std::string& name, const std::string& value)
{
	query_item_t item;
	item.name = name;
	item.value = value;
	queries.push_back(item);
}

void sip_address::set(const std::string& name, const std::string& value)
{
	std::vector<query_item_t>::iterator itr = find_mutable(name);
	if (itr == queries.end())
	{
		add(name, value);
	}
	else
	{
		itr->value = value;
	}
}

bool sip_address::get(const std::string& name, std::string& value)const
{
	auto itr = find(name);
	if (itr == queries.end())
	{
		return false;
	}
	value = itr->value;
	return true;
}

void sip_address::get(const std::string& name, std::vector<std::string>& values)const
{
	values.clear();
	for (auto itr = queries.begin(); itr != queries.end(); itr++)
	{
		if (sys::string_util::icasecompare(name, itr->name))
		{
			values.push_back(itr->value);
		}
	}
}

bool sip_address::exist(const std::string& name)
{
	for (auto itr = queries.begin(); itr != queries.end(); itr++)
	{
		if (sys::string_util::icasecompare(name, itr->name))
		{
			return true;
		}
	}
	return false;
}

void sip_address::remove(const std::string& name)
{
	for (auto itr = queries.begin(); itr != queries.end();)
	{
		if (sys::string_util::icasecompare(name, itr->name))
		{
			itr = queries.erase(itr);
		}
		else
		{
			itr++;
		}
	}
}

void sip_address::clear()
{
	queries.clear();
}



std::vector<sip_address::query_item_t>::const_iterator sip_address::find(const std::string& name)const
{
	return std::find_if(queries.begin(), queries.end(), [&name](const query_item_t& a) {
		return sys::string_util::icasecompare(a.name, name);
		});
}

std::vector<sip_address::query_item_t>::iterator sip_address::find_mutable(const std::string& name)
{
	return std::find_if(queries.begin(), queries.end(), [&name](const query_item_t& a) {
		return sys::string_util::icasecompare(a.name, name);
		});
}
