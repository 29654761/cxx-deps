
/**
 * @file string_util.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once
#include <string>
#include <vector>
#include <codecvt>
#include <locale>
#include <string.h>
#include <algorithm>

#if _MSC_VER>=1400
#define strcasecmp(s1,s2) _stricmp(s1,s2)
#define strncasecmp(s1,s2,n) _strnicmp(s1,s2,n)
#elif defined(_MSC_VER)
#define strcasecmp(s1,s2) stricmp(s1,s2)
#define strncasecmp(s1,s2,n) strnicmp(s1,s2,n)
#endif

namespace sys {

class string_util
{
public:
	static void split(const std::string& src, const std::string& pattern, std::vector<std::string>& result, int max=-1, bool removeEmpty=true);
	static std::vector<std::string> split(const std::string& src, const std::string& pattern, int max=-1, bool removeEmpty=true);
	
	static std::string join(const std::string& separator, const std::vector<std::string>& segments);
	template<typename Arg>
	static void expand(std::vector<Arg>&vec,Arg arg)
	{
		vec.push_back(arg);
	}

	template<typename ...Args>
	static std::string join(const std::string& separator, Args... args)
	{
		std::vector<std::string> segments;
		int arr[] = { (expand<std::string>(segments,args), 0) ...};

		return join(separator, segments);
	}
	
	//Methods for utf8 are unsported on Adnroid NDK
	static std::string gb2312_to_utf8(const std::string& gb2312);
	static std::string utf8_to_gb2312(const std::string& utf8);

	static std::string w2s(const std::wstring& w);
	static std::wstring s2w(const std::string& s);

	static char* alloc_buffer(const std::string& s);
	static void free_buffer(char* p);
	static void freep_buffer(char** p);

	static std::string join_path(const std::string& path1, const std::string& path2, std::string::value_type separator);

	static void ltrim(std::string& s);
	static void rtrim(std::string& s);
	static void trim(std::string& s);

	static bool icasecompare(const std::string& a, const std::string& b);

	static const char* find_str(const char* str, size_t size, const char* substr, size_t subsize);

	static bool is_start_of(const std::string& str, const std::string& prefix);
	static bool is_end_of(const std::string& str,const std::string& suffix);

	static bool is_valid_utf8(const std::string& str);
};

}

