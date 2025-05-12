#pragma once
#include "CommonHeaders.h"

namespace mofu {
using id_t = u32;
}

namespace mofu::id {

namespace detail {
// index/generation system based on Primal Engine
constexpr u32 GENERATION_BITS{ 8 };
constexpr u32 INDEX_BITS{ sizeof(id_t) * 8 - GENERATION_BITS };
constexpr id_t INDEX_MASK{ (id_t{1} << INDEX_BITS) - 1 };
constexpr id_t GENERATION_MASK{ (id_t{1} << GENERATION_BITS) - 1 };
} // namespace detail

constexpr id_t INVALID_ID{ id_t(-1) };
constexpr u32 MIN_DELETED_ELEMENTS{ 1024 };

// set generation_type to the smallest uint type that can hold GENERATION_BITS
using generation_type = std::conditional_t<detail::GENERATION_BITS <= 16, std::conditional_t<detail::GENERATION_BITS <= 8, u8, u16>, u32>;
static_assert(sizeof(generation_type) * 8 >= detail::GENERATION_BITS);
static_assert((sizeof(id_t) - sizeof(generation_type)) > 0);

constexpr generation_type MAX_GENERATION{ (generation_type)(detail::GENERATION_MASK - 1) };

constexpr inline bool
IsValid(id_t id)
{
	return id != INVALID_ID;
}

constexpr inline id_t
Index(id_t forId)
{
	id_t index{ forId & detail::INDEX_MASK };
	assert(index != detail::INDEX_MASK);
	return index;
}

constexpr inline id_t
Generation(id_t forId)
{
	return (forId >> detail::INDEX_BITS) & detail::GENERATION_MASK;
}

#ifdef _DEBUG
namespace detail {
	struct IdBase
	{
		constexpr explicit IdBase(id_t id) : _id{ id } {}
		constexpr operator id_t() const { return _id; }

	private:
		id_t _id;
	};
} // namespace detail

#define DEFINE_TYPED_ID(name)								\
	struct name final : id::detail::IdBase					\
	{														\
		constexpr explicit name(id_t id) : IdBase{id} {}	\
		constexpr name() : IdBase{0} {};					\
	};
#else // RELEASE
#define DEFINE_TYPED_ID(name) using name = id::id;
#endif
}