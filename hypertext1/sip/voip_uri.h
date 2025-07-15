#pragma once
#include <string>
#include <vector>

class voip_uri
{
	typedef struct query_item_t
	{
		std::string name;
		std::string value;
	}query_item_t;

public:
	voip_uri();
	voip_uri(const std::string& uri);
	~voip_uri();

	void parse(const std::string& uri);
	std::string to_string()const;
	std::string to_full_string()const;

	void add(const std::string& name, const std::string& value);
	void set(const std::string& name, const std::string& value);
	bool get(const std::string& name, std::string& value)const;
	void get(const std::string& name, std::vector<std::string>& values)const;
	bool exist(const std::string& name);
	void remove(const std::string& name);
	void clear();

private:
	void parse_host(const std::string& host);
	void parse_query(const std::string& query);
	std::vector<query_item_t>::const_iterator find(const std::string& name)const;
	std::vector<query_item_t>::iterator find_mutable(const std::string& name);
public:
	std::string scheme = "sip";
	std::string username;
	std::string host;
	int port = 0;
	std::vector<query_item_t> queries;
};
