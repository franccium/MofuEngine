#pragma once
#include "CommonHeaders.h"
#include "Component.h"

#if EDITOR_BUILD
#include "imgui.h"
#include "Editor/ValueDisplay.h"
#endif

/*
* Updates to transform will first change the values stored in TransformCache, and the changes will be applied 
* at the end of the frame, when writes to TransformCache are blocked; only the flagged elements will be updated
* this helps with multithreading and lets the different modules (physics engine, various scripts) to read and modify the data
* TODO: module transform update priority
*/

namespace mofu::ecs::component {
// for roots this would be world space, for children local
struct InitInfo
{

};

// used for rendering
struct WorldTransform : Component
{
	m4x4 TRS{};

#if EDITOR_BUILD
	static void RenderFields(WorldTransform& c)
	{
		editor::DisplayMatrix4x4(&c.TRS, "TRS");
	}
#endif
};

struct RenderMesh : Component
{
	m4x4 TRS{};

#if EDITOR_BUILD
	static void RenderFields(RenderMesh& c)
	{
		editor::DisplayMatrix4x4(&c.TRS, "Render Mesh");
	}
#endif
};

struct RenderMaterial : Component
{
	m4x4 TRS{};
#if EDITOR_BUILD
	static void RenderFields(RenderMaterial& c)
	{
		editor::DisplayMatrix4x4(&c.TRS, "Render Material");
	}
#endif
};

struct Parent : Component
{
	Entity ParentEntity{ id::INVALID_ID };
#if EDITOR_BUILD
	static void RenderFields(Parent& c)
	{
		ImGui::InputScalar("Data2", ImGuiDataType_U32, &c.ParentEntity);
	}
#endif
};

// exposed to various systems as read-only or read-write
struct LocalTransform : Component
{
	v3 Position{ 0.0f, 0.0f, 0.0f };
	v3 Rotation{ 0.0f, 0.0f, 0.0f };
	v3 Scale{ 1.0f, 1.0f, 1.0f };
	v3 Forward{ 0.0f, 0.0f, 1.0f };
#if EDITOR_BUILD
	static void RenderFields(LocalTransform& c)
	{
		ImGui::TableNextRow();
		ImGui::TextUnformatted("Local Transform");
		ImGui::TableNextRow();
		editor::DisplayEditableVector3(&c.Position, "Position");
		ImGui::TableNextRow();
		editor::DisplayEditableVector3(&c.Rotation, "Rotation");
		ImGui::TableNextRow();
		editor::DisplayEditableVector3(&c.Scale, "Scale");
	}
#endif
};

}