#pragma once
#include "CommonHeaders.h"
#include <stack>

namespace mofu::editor {

// TODO: most likely gonna need UI code

//struct Action
//{
//	void Undo() {};
//	void Redo() {};
//};

// spatial concerns
//template <typename T>
//struct StaticActionT : Action
//{
//	T oldValue;
//	T* currentPtr;
//};

// since component data can move around in memory
struct ComponentAction
{

};

//template <typename T>
//void RegisterStaticAction(const StaticAction<T>& action)
//{
//
//}

void RegisterStaticAction(u32 oldValue, u32* currentPtr);

template <typename T>
void 
RegisterStaticAction(T oldValue, T* currentPtr)
{
	static_assert(sizeof(T) == sizeof(u32));
	static_assert(std::is_trivially_copyable_v<T>);
	u32 temp{};
	std::memcpy(&temp, &oldValue, sizeof(u32));
	RegisterStaticAction(temp, reinterpret_cast<u32*>(currentPtr));
}

void Undo();
void Redo();

}