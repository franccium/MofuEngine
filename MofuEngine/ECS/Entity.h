#pragma once
#include "CommonHeaders.h"

/*
* scene-unique identifier
*/

namespace mofu::ecs
{
DEFINE_TYPED_ID(entity_id)
struct Entity
{
	entity_id ID;
};
}