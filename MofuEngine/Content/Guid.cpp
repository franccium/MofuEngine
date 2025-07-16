#include "Guid.h"
#include <random>

namespace mofu {
namespace {

std::random_device randomDevice{};
std::mt19937_64 mt{ randomDevice() };
std::uniform_int_distribution<u64> uniformDistribution{};

} // anonymous namespace

Guid::Guid() : id{ uniformDistribution(mt) }
{
}

Guid::Guid(u64 id) : id(id)
{
}

}