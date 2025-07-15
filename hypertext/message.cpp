#include "message.h"
#include <sys2/string_util.h>
#include <sstream>

namespace hypertext
{

	message::message()
	{
	}

	message::~message()
	{
	}

	int64_t message::content_length()const
	{
		std::string value;
		if (!headers.get("Content-Length", value))
		{
			return 0;
		}
		char* endptr = nullptr;
		return strtoll(value.c_str(), &endptr, 0);
	}

	std::string message::content_type()const
	{
		std::string value;
		headers.get("Content-Type", value);
		return value;
	}

	void message::set_content_type(const std::string& content_type)
	{
		headers.set("Content-Type", content_type);
	}

	bool message::set_first_line(const std::string& line)
	{

		std::vector<std::string> vec = sys::string_util::split(line, " ", 3);
		if (vec.size() < 3)
		{
			return false;
		}

		filed1 = vec[0];
		filed2 = vec[1];
		filed3 = vec[2];
		sys::string_util::trim(filed1);
		sys::string_util::trim(filed3);
		return true;
	}

	bool message::set_header_line(const std::string& line)
	{
		std::vector<std::string> vec = sys::string_util::split(line, ":", 2);
		if (vec.size() < 2)
		{
			return false;
		}

		sys::string_util::trim(vec[0]);
		sys::string_util::trim(vec[1]);

		headers.add(vec[0], vec[1]);
		return true;
	}

	void message::set_body(const std::string& body)
	{
		this->body = body;
		headers.set("Content-Length", std::to_string(body.size()));
	}


	bool message::parse(const std::string& data)
	{
		reset();
		auto all = sys::string_util::split(data, "\r\n\r\n", 2, false);
		if (all.size() < 2)
		{
			headers.clear();
			return false;
		}

		auto lines = sys::string_util::split(all[0], "\r\n", -1, false);
		headers.clear();
		for (auto itr = lines.begin(); itr != lines.end(); itr++)
		{
			if (itr == lines.begin())
			{
				if (!set_first_line(*itr))
				{
					headers.clear();
					return false;
				}
			}
			else
			{
				if (!set_header_line(*itr))
				{
					headers.clear();
					return false;
				}
			}
		}

		body = all[1];
		return true;
	}
	/*
	int64_t message::parse(const std::string& data, size_t offset)
	{
		size_t pos=data.find("\r\n\r\n", offset);
		if (pos == std::string::npos)
		{
			headers.clear();
			return 0;
		}

		int64_t hdclen = 0;
		if (!base_headers::parse_content_length(data, offset, hdclen))
		{
			headers.clear();
			return -1;
		}

		int64_t clen = data.size() - pos - 4;
		if (hdclen > clen)
		{
			headers.clear();
			return 0;
		}

		size_t header_size = pos - offset;
		std::string header = data.substr(offset, header_size);
		auto lines = sys::string_util::split(header, "\r\n", -1, false);
		headers.clear();
		for (auto itr = lines.begin(); itr != lines.end(); itr++)
		{
			if (itr == lines.begin())
			{
				if (!set_first_line(*itr))
				{
					headers.clear();
					return -1;
				}
			}
			else
			{
				if (!set_header_line(*itr))
				{
					headers.clear();
					return -1;
				}
			}
		}

		this->body = data.substr(pos+4, hdclen);
		return pos + 4 - offset + hdclen;
	}
	*/
	int64_t message::parse(const char* data, size_t size)
	{
		size_t offset = 0;
		for (size_t i = 0; i < size; i++)
		{
			if (data[i] != ' ' && data[i] != '\r' && data[i] != '\n')
			{
				break;
			}
			offset++;
		}

		const char* pos = sys::string_util::find_str(data + offset, size, "\r\n\r\n", 4);
		if (!pos)
		{
			headers.clear();
			return offset;
		}

		int64_t hdclen = 0;
		headers::parse_content_length(data, size, hdclen);

		int64_t clen = size - (pos - (data + offset)) - 4;
		if (hdclen > clen)
		{
			headers.clear();
			return offset;
		}

		size_t header_size = pos - (data + offset);
		std::string header(data + offset, header_size);
		auto lines = sys::string_util::split(header, "\r\n", -1, false);
		headers.clear();
		for (auto itr = lines.begin(); itr != lines.end(); itr++)
		{
			if (itr == lines.begin())
			{
				if (!set_first_line(*itr))
				{
					headers.clear();
					return -1;
				}
			}
			else
			{
				if (!set_header_line(*itr))
				{
					headers.clear();
					return -1;
				}
			}
		}

		this->body.assign(pos + 4, hdclen);
		return header_size + 4 + hdclen + offset;
	}

	std::string message::to_string()const
	{
		std::stringstream ss;

		std::string f2;
		if (is_request()) {
			uri u;
			u.parse(filed2);
			f2 = u.to_path_string();
		}
		else {
			f2 = filed2;
		}

		ss << filed1 << " " << f2 << " " << filed3 << "\r\n";
		for (auto itr = headers.items.begin(); itr != headers.items.end(); itr++)
		{
			ss << itr->name << ": " << itr->value << "\r\n";
		}

		ss << "\r\n";
		ss << body;
		return ss.str();
	}

	void message::reset()
	{
		filed1 = "";
		filed2 = "";
		filed3 = "";
		headers.clear();
		body.clear();
	}

	bool message::is_request()const
	{
		return filed2.find_first_of('/', 0) != std::string::npos;
	}

	bool message::match_method(const std::string& method)const
	{
		return sys::string_util::icasecompare(this->method(), method);
	}

}


