#include "voip_uri.h"
#include <sys2/string_util.h>
#include <sstream>

voip_uri::voip_uri()
{
}
voip_uri::voip_uri(const std::string& uri)
{
	parse(uri);
}

voip_uri::~voip_uri()
{
}

void voip_uri::parse(const std::string& uri)
{
	auto ss=sys::string_util::split(uri, ";", 2);
	if (ss.size() >= 2)
	{
		parse_host(ss[0]);
		parse_query(ss[1]);
	}
	else if(ss.size()>=1)
	{
		parse_host(ss[0]);
	}
}

void voip_uri::parse_host(const std::string& host)
{
	char* endptr = nullptr;
	std::string name_host_port;
	if (strncasecmp(host.c_str(), "sip:", 4) == 0) {
		scheme = "sip";
		name_host_port = host.substr(4);
	}
	else if (strncasecmp(host.c_str(), "h323:", 5) == 0) {
		scheme = "h323";
		name_host_port = host.substr(5);
	}
	else {
		name_host_port = host;
	}

	auto ss=sys::string_util::split(name_host_port, "@", 2,false);
	if (ss.size() >= 2) {
		this->username = ss[0];
		auto ss2 = sys::string_util::split(ss[1], ":", 2);
		if (ss2.size() > 0)
		{
			this->host = ss2[0];
			if (ss2.size() >= 2) {
				this->port = strtol(ss2[1].c_str(), &endptr, 10);
			}
		}
	}
	else {
		auto ss2 = sys::string_util::split(name_host_port, ":", 2,false);
		if (ss2.size() >= 2) {
			this->host = ss2[0];
			this->port = strtol(ss2[1].c_str(), &endptr, 10);
		}
		else if(ss2.size()>=1){
			this->username = ss2[0];
		}
	}
}

void voip_uri::parse_query(const std::string& query)
{
	queries.clear();
	auto ss = sys::string_util::split(query, ";");
	for (auto itr = ss.begin(); itr != ss.end(); itr++)
	{
		query_item_t q;
		auto ss2=sys::string_util::split(*itr, "=",2);
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

std::string voip_uri::to_string()const
{
	std::stringstream ss;
	if (!scheme.empty()) {
		ss << scheme << ":";
	}
	if (username.size() > 0)
	{
		ss << username;
	}
	if (username.size() > 0 && host.size() > 0)
	{
		ss << "@";
	}
	if (host.size() > 0)
	{
		ss << host;
	}
	if (port > 0)
	{
		ss << ":" << port;
	}
	for (auto itr = queries.begin(); itr != queries.end(); itr++)
	{
		ss << ";" << itr->name << "=" << itr->value;
	}
	return ss.str();
}

std::string voip_uri::to_full_string()const
{
	std::stringstream ss;
	ss << scheme << ":";
	ss << username << "@";

	ss << host;
	if (port > 0)
	{
		ss << ":" << port;
	}
	for (auto itr = queries.begin(); itr != queries.end(); itr++)
	{
		ss << ";" << itr->name << "=" << itr->value;
	}
	return ss.str();
}

void voip_uri::add(const std::string& name, const std::string& value)
{
	query_item_t item;
	item.name = name;
	item.value = value;
	queries.push_back(item);
}

void voip_uri::set(const std::string& name, const std::string& value)
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

bool voip_uri::get(const std::string& name, std::string& value)const
{
	auto itr = find(name);
	if (itr == queries.end())
	{
		return false;
	}
	value = itr->value;
	return true;
}

void voip_uri::get(const std::string& name, std::vector<std::string>& values)const
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

bool voip_uri::exist(const std::string& name)
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

void voip_uri::remove(const std::string& name)
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

void voip_uri::clear()
{
	queries.clear();
}



std::vector<voip_uri::query_item_t>::const_iterator voip_uri::find(const std::string& name)const
{
	return std::find_if(queries.begin(), queries.end(), [&name](const query_item_t& a) {
		return sys::string_util::icasecompare(a.name, name);
		});
}

std::vector<voip_uri::query_item_t>::iterator voip_uri::find_mutable(const std::string& name)
{
	return std::find_if(queries.begin(), queries.end(), [&name](const query_item_t& a) {
		return sys::string_util::icasecompare(a.name, name);
		});
}

