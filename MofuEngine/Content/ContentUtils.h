#pragma once
#include "CommonHeaders.h"
#include <filesystem>
#include <fstream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <wrl.h>

namespace mofu::content {

struct Color
{
	f32 r;
	f32 g;
	f32 b;
	f32 a{ 1.f };

	constexpr bool IsTransparent() const { return a < 0.001f; }
	constexpr bool IsBlack() const { return r < 0.001f && g < 0.001f && b < 0.001f; }

	constexpr Color LinearToSRGB()
	{
		return { r < 0.0031308f ? 12.92f * r : f32((1.0 + 0.055) * std::pow(r, 1.0f / 2.4f) - 0.055),
			g < 0.0031308f ? 12.92f * g : f32((1.0 + 0.055) * std::pow(g, 1.0f / 2.4f) - 0.055),
			b < 0.0031308f ? 12.92f * b : f32((1.0 + 0.055) * std::pow(b, 1.0f / 2.4f) - 0.055),
			a };
	}

	constexpr Color SRGBToLinear()
	{
		return { r < 0.04045f ? r * (1.0f / 12.92f) : std::pow(f32((r + 0.055) * (1.0 / (1.0 + 0.055))), 2.4f),
			g < 0.04045f ? g * (1.0f / 12.92f) : std::pow(f32((g + 0.055) * (1.0 / (1.0 + 0.055))), 2.4f),
			b < 0.04045f ? b * (1.0f / 12.92f) : std::pow(f32((b + 0.055) * (1.0 / (1.0 + 0.055))), 2.4f),
			a };
	}

	constexpr Color operator+(Color c)
	{
		r += c.r; g += c.g; b += c.b; a += c.a;
		return *this;
	}
	constexpr Color operator+=(Color c) { return (*this) + c; }

	constexpr Color operator*(f32 s)
	{
		r *= s; g *= s; b *= s; a *= s;
		return *this;
	}
	constexpr Color operator*=(f32 s) { return (*this) * s; }
	constexpr Color operator/=(f32 s) { return (*this) * (1.f / s); }
};

inline Vec<std::string>
SplitString(std::string s, char delimiter)
{
	size_t start{ 0 };
	size_t end{ 0 };
	std::string substring;
	Vec<std::string> strings;

	while ((end = s.find(delimiter, start)) != std::string::npos)
	{
		//TODO: can i do a non-owning substr
		substring = s.substr(start, end - start);
		start = end + sizeof(char);
		strings.emplace_back(substring);
	}
	substring = s.substr(start);
	if(!substring.empty())
		strings.emplace_back(substring);

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

inline void
ListFilesByExtensionRec(const char* extension, const std::filesystem::path fromFolder, Vec<std::string>& outFiles)
{
	outFiles.clear();
	for (auto& entry : std::filesystem::recursive_directory_iterator(fromFolder))
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