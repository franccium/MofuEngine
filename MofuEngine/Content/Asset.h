#pragma once
#include "CommonHeaders.h"
#include "Guid.h"

namespace mofu::content {
using AssetHandle = Guid;

struct AssetType
{
	enum type : u32
	{
		Unknown = 0,
		Mesh,
		Texture,
		Animation,
		Audio,
		Material,
		Skeleton,

		Count
	};
};

struct Asset
{
	AssetType::type Type;
	std::filesystem::path FilePath;
};

}