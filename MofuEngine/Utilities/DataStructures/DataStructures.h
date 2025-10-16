#pragma once
#include "Vector.h"
#include "FreeList.h"
#include "Array.h"
#include <deque>

namespace mofu::util {
}

namespace mofu {
template<typename T>
using Vec = util::Vector<T, true>;

template<typename T>
using Deque = std::deque<T>;

//TODO: make a nice hashmap
template<typename T, typename U>
using HashMap = std::unordered_map<T, U>;

}