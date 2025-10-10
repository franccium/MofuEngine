#include "SystemMessages.h"

namespace mofu::ecs::messages {
namespace {
bool _boolMessages[SystemBoolMessage::Count]{ false };
}

void
SetMessage(SystemBoolMessage::Type type, bool value)
{
	_boolMessages[type] = value;
}

bool
GetBoolMessage(SystemBoolMessage::Type type)
{
	return _boolMessages[type];
}

void 
RestartFrameMessages()
{
	for(u32 i{0}; i < SystemBoolMessage::Count; ++i)
	{
		_boolMessages[i] = false;
	}
}

}