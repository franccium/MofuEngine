#pragma once
#include "CommonHeaders.h"
#include <filesystem>
#include <fstream>

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

inline void
ListFilesByExtension(const char* extension, const std::filesystem::path fromFolder, Vec<std::string>& outFiles)
{
	outFiles.clear();
	for (auto& entry : std::filesystem::directory_iterator(fromFolder))
	{
		if (entry.is_regular_file() && entry.path().extension() == extension)
		{
			outFiles.push_back(entry.path().string());
		}
	}
}

inline bool
ReadFileToByteBuffer(std::filesystem::path path, std::unique_ptr<u8[]>& outData, u64& outFileSize)
{
	if (!std::filesystem::exists(path)) return false;
	outFileSize = std::filesystem::file_size(path);
	assert(outFileSize != 0);

	outData = std::make_unique<u8[]>(outFileSize);
	std::ifstream file{ path, std::ios::in | std::ios::binary };
	if (!file || !file.read((char*)outData.get(), outFileSize))
	{
		file.close();
		return false;
	}
	file.close();
	return true;
}

}