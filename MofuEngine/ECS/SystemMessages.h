#pragma once
#include "CommonHeaders.h"

namespace mofu::ecs::messages {
//TODO: something better than an enum boolean
struct SystemBoolMessage
{
	enum Type : u8
	{
		TransformChanged = 0,
		MeshChanged,
		MaterialChanged,
		LightChanged,
		CameraChanged,

		Count
	};
};

void SetMessage(SystemBoolMessage::Type type, bool value);
bool GetBoolMessage(SystemBoolMessage::Type type);
void RestartFrameMessages();
}