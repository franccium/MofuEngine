#include "ActionHistory.h"

namespace mofu::editor {
namespace {
struct StaticAction
{
	u32 OldValue;
	u32* CurrentPtr;

	constexpr StaticAction(u32 oldValue, u32* currentPtr) 
		: OldValue{ oldValue }, CurrentPtr{ currentPtr } {}
};

std::stack<StaticAction> undoActions;
std::stack<StaticAction> redoActions;
}

void 
RegisterStaticAction(u32 oldValue, u32* currentPtr)
{
	undoActions.emplace(StaticAction{ oldValue, reinterpret_cast<u32*>(currentPtr) });
}

void
Undo()
{
	if (undoActions.empty()) return;
	const StaticAction& action = undoActions.top();
	undoActions.pop();
	redoActions.emplace(StaticAction{ *action.CurrentPtr, action.CurrentPtr });
	*action.CurrentPtr = action.OldValue;
}

void 
Redo()
{
	if (redoActions.empty()) return;
	const StaticAction& action = redoActions.top();
	u32 currentValue{ *action.CurrentPtr };
	redoActions.pop();
	undoActions.emplace(StaticAction{ currentValue, action.CurrentPtr });
	*action.CurrentPtr = action.OldValue;
}
}