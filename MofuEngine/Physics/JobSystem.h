#pragma once
#include "CommonHeaders.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>

namespace mofu::physics::jobs {
//NOTE: for now just use jolt's implementation
using JobSystem = JPH::JobSystemThreadPool;

}