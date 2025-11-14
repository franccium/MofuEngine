#include "ObjectAddInterface.h"
#include "Input/InputSystem.h"
#include <imgui.h>
#include "Content/Asset.h"
#include "SceneEditorView.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include "Physics/PhysicsCore.h"
#include "Physics/PhysicsLayers.h"
#include "Physics/BodyManager.h"
#include <Jolt/Physics/Body/BodyLock.h>
#include "ECS/Transform.h"
#include "ECS/Scene.h"

namespace mofu::editor::object {
namespace {
bool _isOpen{ false };
const content::AssetHandle PHYSICS_CUBE_HANDLE{ 24242424 };
const content::AssetHandle PHYSICS_SPHERE_HANDLE{ 24242425 };

struct Objects
{
	enum Type : u8
	{
		PhysicsCube,
		PhysicsSphere,
		Count
	};
};

constexpr const char* _prefabPaths[Objects::Count] = {
	"EditorAssets/Prefabs/pcube.pre",
	"EditorAssets/Prefabs/psphere.pre",
};

void AddPhysicsCube(v2 mousePos)
{
	log::Error("AddPhysicsCube Turned off");
	return;
	ecs::Entity e{ editor::AddPrefab(_prefabPaths[Objects::PhysicsCube]) };
	JPH::BoxShape boxShape{ JPH::Vec3{1.f, 1.f, 1.f} };
	JPH::Shape* shape{ new JPH::BoxShape{JPH::Vec3{1.f, 1.f, 1.f}} };
	JPH::BodyID id{ physics::AddDynamicBody(shape, e) };

	JPH::BodyLockWrite lock{ physics::core::PhysicsSystem().GetBodyLockInterface(), id };
	lock.GetBody().GetMotionProperties()->SetGravityFactor(0.1f);
}

void AddPhysicsSphere(v2 mousePos)
{
	editor::AddPrefab(_prefabPaths[Objects::PhysicsSphere]);
}

using ObjectAdders = void(*)(v2 mousePos);
constexpr ObjectAdders _objectAdders[Objects::Count]{
	AddPhysicsCube,
	AddPhysicsSphere,
};

constexpr const char* _objectNames[Objects::Count] = {
	"Physics Cube",
	"Physics Sphere"
};
} // anonymous namespace

void 
RenderObjectAddInterface()
{
	v2 mousePos{ input::GetMousePosition() };
	if (input::WasKeyPressed(input::Keybinds::Editor.ToggleObjectAdder))
	{
		_isOpen = !_isOpen;
		if (_isOpen)
		{
			ImGui::SetNextWindowPos(ImVec2(mousePos.x, mousePos.y));
			ImGui::SetNextWindowFocus();
		}
	}
	if (!_isOpen) return;

	ImGui::Begin("Add Object", &_isOpen, ImGuiWindowFlags_AlwaysAutoResize);
	for (u32 i{ 0 }; i < Objects::Count; ++i)
	{
		if (ImGui::Selectable(_objectNames[i]))
		{
			_objectAdders[i](mousePos);
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::End();
}
}