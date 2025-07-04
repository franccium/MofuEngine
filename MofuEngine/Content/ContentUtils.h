#pragma once
#include "CommonHeaders.h"
#include <filesystem>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <wrl.h>

namespace mofu::content {

inline Vec<std::string>
SplitString(std::string s, char delimiter)
{
	size_t start{ 0 };
	size_t end{ 0 };
	std::string substring;
	Vec<std::string> strings;

	while ((end = s.find(delimiter, start)) != std::string::npos)
	{
		substring = s.substr(start, end - start);
		start = end + sizeof(char);
		strings.emplace_back(substring);
	}
	strings.emplace_back(s.substr(start));

	return strings;
}

inline std::wstring
toWstring(const char* cstr)
{
	std::string s{ cstr };
	return { s.begin(), s.end() };
}

}