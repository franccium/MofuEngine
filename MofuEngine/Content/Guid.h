#pragma once
#include "CommonHeaders.h"

namespace mofu {
struct Guid
{
	u64 id;
	
	Guid();
	Guid(u64 id);
	operator u64() const { return id; }
};
}

namespace std {
template<>
struct hash<mofu::Guid>
{
	size_t operator()(const mofu::Guid& guid) const
	{
		return hash<u64>()((u64)guid);
	}
};
}