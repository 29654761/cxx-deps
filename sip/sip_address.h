#pragma once
#include "voip_uri.h"

class sip_address
{
	typedef struct query_item_t
	{
		std::string name;
		std::string value;
	}query_item_t;
public:
	sip_address();
	sip_address(const std::string& display,const voip_uri& url);
	~sip_address();

	void parse(const std::string& s);
	std::string to_string()const;

	void add(const std::string& name, const std::string& value);
	void set(const std::string& name, const std::string& value);
	bool get(const std::string& name, std::string& value)const;
	void get(const std::string& name, std::vector<std::string>& values)const;
	bool exist(const std::string& name);
	void remove(const std::string& name);
	void clear();

private:
	std::vector<query_item_t>::const_iterator find(const std::string& name)const;
	std::vector<query_item_t>::iterator find_mutable(const std::string& name);
public:
	std::string display;
	voip_uri url;
	std::vector<query_item_t> queries;
};

