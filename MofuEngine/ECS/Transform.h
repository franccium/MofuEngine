#pragma once
#include "CommonHeaders.h"
#include "Component.h"

#if EDITOR_BUILD
#include "imgui.h"
#include "imgui_internal.h"
#include "Editor/ValueDisplay.h"
#include "ImGuizmo.h"
#include "Content/Asset.h"
#include "EngineAPI/Camera.h"
#include "Graphics/Renderer.h"
#include "Content/SerializationUtils.h"
#endif

/*
* Updates to transform will first change the values stored in TransformCache, and the changes will be applied 
* at the end of the frame, when writes to TransformCache are blocked; only the flagged elements will be updated
* this helps with multithreading and lets the different modules (physics engine, various scripts) to read and modify the data
* TODO: module transform update priority
*/

namespace mofu::ecs::component {
constexpr u32 NAME_LENGTH{ 16 };
	
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
	content::AssetHandle MeshAsset;
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
	content::AssetHandle MaterialAsset;
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

		static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
		static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
		ImGui::InputFloat3("Tr", &c.Position.x);
		ImGui::InputFloat3("Rt", &c.Rotation.x);
		ImGui::InputFloat3("Sc", &c.Scale.x);
		matrix_t matrix{ };
		ImGuizmo::RecomposeMatrixFromComponents(&c.Position.x, &c.Rotation.x, &c.Scale.x, matrix.m16);

		if (mCurrentGizmoOperation != ImGuizmo::SCALE)
		{
			if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
				mCurrentGizmoMode = ImGuizmo::LOCAL;
			ImGui::SameLine();
			if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
				mCurrentGizmoMode = ImGuizmo::WORLD;
		}
		static bool useSnap(false);
		
		ImGuiIO& io = ImGui::GetIO();
		bool useWindow = true;
		float viewManipulateRight = io.DisplaySize.x;
		float viewManipulateTop = 0;
		static ImGuiWindowFlags gizmoWindowFlags = 0;
		ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Appearing);
		ImGui::SetNextWindowPos(ImVec2(400, 20), ImGuiCond_Appearing);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, (ImVec4)ImColor(0.35f, 0.3f, 0.3f));
		if (useWindow)
		{
			ImGui::Begin("Gizmo", 0, gizmoWindowFlags);
			ImGuizmo::SetDrawlist();
		}
		float windowWidth = (float)ImGui::GetWindowWidth();
		float windowHeight = (float)ImGui::GetWindowHeight();

		if (!useWindow)
		{
			ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
		}
		else
		{
			ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);
		}


		const graphics::Camera& cam{ graphics::GetMainCamera() };
		m4x4 camView{ cam.View() };
		float* camViewF{ &camView._11 };
		m4x4 camProj{ cam.Projection() };
		float* camProjF{ &camProj._11 };
		matrix_t id{ matrix_t::Identity() };

		viewManipulateRight = ImGui::GetWindowPos().x + windowWidth;
		viewManipulateTop = ImGui::GetWindowPos().y;
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		gizmoWindowFlags = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(window->InnerRect.Min, window->InnerRect.Max) ? ImGuiWindowFlags_NoMove : 0;

		ImGuizmo::DrawGrid(camViewF, camProjF, id.m16, 100.f);
		ImGuizmo::DrawCubes(camViewF, camProjF, matrix.m16, 1);

		ImGuizmo::ViewManipulate(camViewF, 100.f, ImVec2(viewManipulateRight - 128, viewManipulateTop), ImVec2(128, 128), 0x10101010);


		//EDIT
		if (!useWindow)
		{
			ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
		}
		else
		{
			ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);
		}
		ImGuizmo::Manipulate(camViewF, camProjF, mCurrentGizmoOperation, mCurrentGizmoMode, matrix.m16, NULL, NULL);

		float currentPosX{ c.Position.x };
		ImGuizmo::DecomposeMatrixToComponents(matrix.m16, &c.Position.x, &c.Rotation.x, &c.Scale.x);

		// END
		if (useWindow)
		{
			ImGui::End();
		}
		ImGui::PopStyleColor(1);
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
	f32 Range{ 5.f };
	v3 Attenuation{ 1.f, 1.f, 1.f };

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
	f32 Range{ 5.f };
	v3 Attenuation{ 1.f, 1.f, 1.f };
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


struct NameComponent : Component
{
	char Name[NAME_LENGTH];

#if EDITOR_BUILD
	static void RenderFields([[maybe_unused]] NameComponent& c)
	{
		ImGui::TableNextRow();
		ImGui::TextUnformatted("Name");
		ImGui::TableNextRow();
		ImGui::TextUnformatted(c.Name);
	}
#endif
};

#if EDITOR_BUILD
inline YAML::Emitter& operator<<(YAML::Emitter& out, const LocalTransform& lt)
{
	//out << YAML::Key << "LocalTransform" << YAML::BeginMap;
	out << YAML::BeginMap;
	out << YAML::Key << "Position" << YAML::Value << lt.Position;
	out << YAML::Key << "Rotation" << YAML::Value << lt.Rotation;
	out << YAML::Key << "Scale" << YAML::Value << lt.Scale;
	out << YAML::Key << "Forward" << YAML::Value << lt.Forward;
	out << YAML::EndMap;
	return out;
}

inline YAML::Emitter& operator<<(YAML::Emitter& out, const Child& wt)
{
	out << YAML::BeginMap;
	out << YAML::Key << "Parent" << YAML::Value << wt.ParentEntity;
	out << YAML::EndMap;
	return out;
}

inline YAML::Emitter& operator<<(YAML::Emitter& out, const RenderMesh& wt)
{
	out << YAML::BeginMap;
	out << YAML::Key << "Mesh" << YAML::Value << wt.MeshAsset;
	out << YAML::EndMap;
	return out;
}

inline YAML::Emitter& operator<<(YAML::Emitter& out, const RenderMaterial& wt)
{
	out << YAML::BeginMap;
	out << YAML::Key << "Material" << YAML::Value << wt.MaterialAsset;
	out << YAML::EndMap;
	return out;
}

inline YAML::Emitter& operator<<(YAML::Emitter& out, const Light& wt)
{
	out << YAML::BeginMap;
	out << YAML::Key << "Intensity" << YAML::Value << wt.Intensity;
	out << YAML::Key << "Color" << YAML::Value << wt.Color;
	out << YAML::Key << "Enabled" << YAML::Value << wt.Enabled;
	out << YAML::EndMap;
	return out;
}

inline YAML::Emitter& operator<<(YAML::Emitter& out, const PointLight& wt)
{
	out << YAML::BeginMap;
	out << YAML::Key << "Range" << YAML::Value << wt.Range;
	out << YAML::Key << "Attenuation" << YAML::Value << wt.Attenuation;
	out << YAML::EndMap;
	return out;
}

inline YAML::Emitter& operator<<(YAML::Emitter& out, const SpotLight& wt)
{
	out << YAML::BeginMap;
	out << YAML::Key << "Range" << YAML::Value << wt.Range;
	out << YAML::Key << "Attenuation" << YAML::Value << wt.Attenuation;
	out << YAML::Key << "Umbra" << YAML::Value << wt.Umbra;
	out << YAML::Key << "Penumbra" << YAML::Value << wt.Penumbra;
	out << YAML::EndMap;
	return out;
}

inline YAML::Emitter& operator<<(YAML::Emitter& out, const NameComponent& wt)
{
	out << YAML::BeginMap;
	out << YAML::Key << "Name" << YAML::Value << wt.Name;
	out << YAML::EndMap;
	return out;
}

#endif


}