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
	static void RenderFields([[maybe_unused]] WorldTransform& c)
	{
		editor::DisplayMatrix4x4(&c.TRS, "TRS");
	}
#endif
};

struct RenderMesh : Component
{
	id_t MeshID{ id::INVALID_ID };

#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] RenderMesh& c)
	{
		editor::DisplayUint(c.MeshID, "Mesh ID");
	}
#endif
};

struct RenderMaterial : Component
{
	u32 MaterialCount{ 0 };
	id_t* MaterialIDs{ nullptr };

#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] RenderMaterial& c)
	{
		if (c.MaterialIDs == nullptr) return;
		editor::DisplayUint(c.MaterialIDs[0], "Material ID");
	}
#endif
};

struct StaticObject : Component // NOTE: for static mesh render optimisation, not yet sure how to handle
{
#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] StaticObject& c)
	{
	}
#endif
};

struct PotentiallyVisible : Component // NOTE: for culling
{
#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] PotentiallyVisible& c)
	{
	}
#endif
};

struct Parent : Component
{
#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] Parent& c)
	{
		ImGui::Text("Parent");
	}
#endif
};

struct Child : Component
{
	Entity ParentEntity{ id::INVALID_ID };
#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] Child& c)
	{
		ImGui::InputScalar("Data2", ImGuiDataType_U32, &c.ParentEntity);
	}
#endif
};

constexpr inline v3 ToRadians(const v3& degrees)
{
	using namespace DirectX;
	return v3{
		XMConvertToRadians(degrees.x),
		XMConvertToRadians(degrees.y),
		XMConvertToRadians(degrees.z)
	};
}

constexpr inline v3 ToDegrees(const v3& radians)
{
	using namespace DirectX;
	return v3{
		XMConvertToDegrees(radians.x),
		XMConvertToDegrees(radians.y),
		XMConvertToDegrees(radians.z)
	};
}


// exposed to various systems as read-only or read-write
struct LocalTransform : Component
{
	v3 Position{ 0.0f, 0.0f, 0.0f };
	quat Rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
	v3 Scale{ 1.0f, 1.0f, 1.0f };
	v3 Forward{ 0.0f, 0.0f, 1.0f };

#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] LocalTransform& c)
	{
		constexpr f32 minPosVal{ -100.f };
		constexpr f32 maxPosVal{ 100.f };
		constexpr f32 minRotVal{ -1.f };
		constexpr f32 maxRotVal{ 1.f };
		constexpr f32 minScaleVal{ -25.f };
		constexpr f32 maxScaleVal{ 100.f };

		ImGui::TableNextRow();
		ImGui::TextUnformatted("Local Transform");
		ImGui::TableNextRow();
		editor::DisplayEditableVector3(&c.Position, "Position", minPosVal, maxPosVal);
		ImGui::TableNextRow();
		editor::DisplayEditableVector4(&c.Rotation, "Rotation", minRotVal, maxRotVal);
		ImGui::TableNextRow();
		editor::DisplayEditableVector3(&c.Scale, "Scale", minScaleVal, maxScaleVal);
	}
#endif
};

struct Camera : Component
{
	v3 TargetPos{};
	v3 TargetRot{};
#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] Camera& c)
	{

	}
#endif
};

struct Light : Component
{
	f32 Intensity{ 1.f };
	v3 Color{ 1.f, 1.f, 1.f };

	//TODO: might want a table
	bool Enabled{ true };

#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] Light& c)
	{
		ImGui::TableNextRow();
		ImGui::TextUnformatted("Light");
		ImGui::TableNextRow();
		editor::DisplayEditableFloat(&c.Intensity, "Intensity", 0.f, 10.f);
		ImGui::TableNextRow();
		editor::DisplayEditableVector3(&c.Color, "Color", 0.f, 1.f);
	}
#endif
};

struct DirtyLight : Component
{
#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] DirtyLight& c)
	{

	}
#endif
};

struct CullableLight : Component
{
#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] CullableLight& c)
	{

	}
#endif
};

struct DirectionalLight : Component
{
#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] DirectionalLight& c)
	{

	}
#endif
};

struct PointLight : Component
{
	v3 Attenuation{ 1.f, 1.f, 1.f };
	f32 Range{ 5.f };

#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] PointLight& c)
	{
		ImGui::TableNextRow();
		ImGui::TextUnformatted("Point Light");
		ImGui::TableNextRow();
		editor::DisplayEditableVector3(&c.Attenuation, "Attenuation", 0.f, 10.f);
		ImGui::TableNextRow();
		editor::DisplayEditableFloat(&c.Range, "Range", 0.f, 50.f);
	}
#endif
};

struct SpotLight : Component
{
	v3 Attenuation{ 1.f, 1.f, 1.f };
	f32 Range{ 5.f };
	f32 Umbra{ 45.f };
	f32 Penumbra{ 45.f };

#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] SpotLight& c)
	{
		ImGui::TableNextRow();
		ImGui::TextUnformatted("Spot Light");
		ImGui::TableNextRow();
		editor::DisplayEditableVector3(&c.Attenuation, "Attenuation", 0.f, 10.f);
		ImGui::TableNextRow();
		editor::DisplayEditableFloat(&c.Range, "Range", 0.f, 50.f);
		ImGui::TableNextRow();
		editor::DisplayEditableFloat(&c.Umbra, "Umbra", 0.f, 45.f);
		ImGui::TableNextRow();
		editor::DisplayEditableFloat(&c.Penumbra, "Penumbra", 0.f, 45.f);
	}
#endif
};

}