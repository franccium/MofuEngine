#include "MaterialEditor.h"
#include "Content/ContentManagement.h"
#include "imgui.h"
#include "Graphics/Renderer.h"
#include "Graphics/UIRenderer.h"
#include "TextureView.h"
#include "ValueDisplay.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "Content/EditorContentManager.h"
#include "Material.h"

#include "Graphics/D3D12/D3D12Core.h"
#include "Project/Project.h"

namespace mofu::editor::material {
namespace {

struct StandardMaterial
{
	enum Flags : u32
	{
		None,
		DiffuseLambert = (1u << 0), // Burley is default
		DisableAmbient = (1u << 1),
		IBLAmbient = (1u << 2),
		Count = 3
	};
	u32 Flags{ Flags::None };
};

constexpr StandardMaterial DEFAULT_STANDARD_MATERIAL{
	StandardMaterial::IBLAmbient,
};

constexpr const wchar_t* STANDARD_MATERIAL_FLAGS_DEFINES[]{
	L"NONE",
	L"DIFFUSE_LAMBERT",
	L"DISABLE_AMBIENT",
	L"AMBIENT_IBL"
};


constexpr const char* WHITE_TEXTURE{ "Projects/TestProject/Resources/Editor/white_placeholder_texture.tex" };
constexpr const char* GRAY_TEXTURE{ "Projects/TestProject/Resources/Editor/gray_placeholder_texture.tex" };
constexpr const char* BLACK_TEXTURE{ "Projects/TestProject/Resources/Editor/black_placeholder_texture.tex" };
constexpr const char* ERROR_TEXTURE{ "Projects/TestProject/Resources/Editor/error_texture.tex" };
constexpr const char* DEFAULT_TEXTURES_PATHS[TextureUsage::Count]{
	ERROR_TEXTURE,
	GRAY_TEXTURE,
	GRAY_TEXTURE,
	BLACK_TEXTURE,
	WHITE_TEXTURE
};
constexpr const char* MATERIAL_SURFACE_TYPE_TO_STRING[graphics::MaterialType::Count]{
	"Opaque",
};
id_t DEFAULT_TEXTURES[TextureUsage::Count];
id_t DEFAULT_SHADERS[shaders::ShaderType::Count]{ id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID };

ecs::Entity materialOwner{};
EditorMaterial editorMaterial{};
content::AssetHandle currentMaterialAsset{ content::INVALID_HANDLE };
graphics::MaterialInitInfo materialInitInfo{};

bool isOpen{ false };
bool isBrowserOpen{ false };
TextureUsage::Usage textureBeingChanged{};

Vec<std::string> texFiles{};
Vec<std::string> matFiles{};

constexpr u32 MAX_NAME_LENGTH{ 16 };
char nameBuffer[MAX_NAME_LENGTH]{};

StandardMaterial standardMaterial{};

void
GenerateStandardMaterial(const StandardMaterial& mat, Vec<const wchar_t*>& extraArgs)
{
	for (u32 i{ 0 }; i < StandardMaterial::Flags::Count; ++i)
	{
		if (mat.Flags & (1u << i))
		{
			extraArgs.emplace_back(L"-D");
			extraArgs.emplace_back(STANDARD_MATERIAL_FLAGS_DEFINES[i]);
		}
	}
}

void
DisplayStandardMaterialEdit(StandardMaterial& mat)
{
	//"Material Properties"
	ImGui::BeginGroup();
	{
		ImGui::TextUnformatted("Flags:");

		constexpr const char* DIFFUSE_OPTIONS[]{ "Burley", "Lambert" };
		static s32 diffOpt{ 0 };
		if (ImGui::ListBox("Diffuse", &diffOpt, DIFFUSE_OPTIONS, _countof(DIFFUSE_OPTIONS)))
		{
			mat.Flags |= (diffOpt & StandardMaterial::Flags::DiffuseLambert);
		}

		ImGui::CheckboxFlags("Disable Ambient", &mat.Flags, StandardMaterial::Flags::DisableAmbient);
		bool ambientDisabled{ (bool)(mat.Flags & StandardMaterial::Flags::DisableAmbient) };
		ImGui::BeginDisabled(ambientDisabled);
		if(ambientDisabled)
			mat.Flags &= ~(StandardMaterial::Flags::IBLAmbient);
		ImGui::CheckboxFlags("IBL Ambient", &mat.Flags, StandardMaterial::Flags::IBLAmbient);
		ImGui::EndDisabled();
	}
	ImGui::EndGroup();
}

[[nodiscard]] id_t
LoadTexture(const char* path)
{
	return content::CreateResourceFromAsset(path, content::AssetType::Texture);
}

void
DisplayTexture(TextureUsage::Usage texUse, const char* label, const char* id)
{
	constexpr ImVec2 imageSize{ 64,64 };

	ImGui::Text(label); ImGui::SameLine();
	if (id::IsValid(editorMaterial.TextureIDs[texUse]))
	{
		if (ImGui::ImageButton(id, graphics::ui::GetImTextureID(editorMaterial.TextureIDs[texUse]), imageSize))
		{
			if (isBrowserOpen)
			{
				ImGui::CloseCurrentPopup();
			}
			texFiles.clear();
			content::ListFilesByExtension(".tex", project::GetResourceDirectory() / "Textures", texFiles);
			ImGui::OpenPopup("SelectTexturePopup");
			isBrowserOpen = true;
			textureBeingChanged = texUse;
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			texture::OpenTextureView(editorMaterial.TextureIDs[texUse]);
		}
	}
	else
	{
		ImGui::PushID(texUse);
		if (ImGui::Button("No Tex", imageSize))
		{
			if (isBrowserOpen)
			{
				ImGui::CloseCurrentPopup();
			}
			texFiles.clear();
			content::ListFilesByExtension(".tex", project::GetResourceDirectory() / "Textures", texFiles);
			ImGui::OpenPopup("SelectTexturePopup");
			isBrowserOpen = true;
			textureBeingChanged = texUse;
		}
		ImGui::PopID();
	}

	content::AssetHandle handle{ content::assets::GetAssetFromResource(editorMaterial.TextureIDs[texUse], content::AssetType::Texture) };
	if (handle != content::INVALID_HANDLE)
	{
		ImGui::TextUnformatted("Path");
		content::AssetPtr asset{ content::assets::GetAsset(handle) };
		auto str{ asset->OriginalFilePath.string() };
		ImGui::TextUnformatted(str.data());
	}
	else
	{
		ImGui::TextUnformatted("No Path Data");
	}
}

void
UpdateMaterialInitInfo()
{
	materialInitInfo = {};
	materialInitInfo.Type = editorMaterial.Type;
	materialInitInfo.Surface = editorMaterial.Surface;
	materialInitInfo.TextureCount = editorMaterial.TextureCount;
	memcpy(materialInitInfo.ShaderIDs, editorMaterial.ShaderIDs, shaders::ShaderType::Count * sizeof(id_t));
	materialInitInfo.TextureIDs = new id_t[editorMaterial.TextureCount];
	memcpy(materialInitInfo.TextureIDs, editorMaterial.TextureIDs, editorMaterial.TextureCount * sizeof(id_t));
	materialInitInfo.MaterialFlags = (graphics::MaterialFlags::Flags)editorMaterial.Flags;
}

void
DuplicateCurrentMaterial()
{
	content::AssetPtr mat{ content::assets::GetAsset(currentMaterialAsset) };
	if (mat)
	{
		std::filesystem::path duplicatePath{ content::CreateUniqueDuplicateName(mat->ImportedFilePath) };
		PackMaterialAsset(editorMaterial, duplicatePath);
		currentMaterialAsset = CreateMaterialAsset(duplicatePath);
		LoadMaterialAsset(editorMaterial, mat->ImportedFilePath);
		UpdateMaterialInitInfo();
	}
	else
	{
		log::Error("Save the current material first");
	}
}

void
RenderTextureBrowser()
{
	if (ImGui::BeginPopup("SelectTexturePopup"))
	{
		for (std::string_view texPath : texFiles)
		{
			if (ImGui::Selectable(texPath.data()))
			{
				content::AssetHandle assetHandle{ content::assets::GetHandleFromImportedPath(texPath) };
				if (assetHandle != content::INVALID_HANDLE)
				{
					editorMaterial.TextureIDs[textureBeingChanged] = content::assets::CreateResourceFromHandle(assetHandle);
				}
				else
				{
					log::Warn("Can't find assetHandle from path");
				}
				//materialInitInfo.TextureIDs[textureBeingChanged] = content::CreateResourceFromAsset(texPath, content::AssetType::Texture);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
}


} // anonymous namespace

bool 
InitializeMaterialEditor()
{
	for (u32 i{ 0 }; i < TextureUsage::Count; ++i)
	{
		//TODO: standardise loading editor default assets
		content::AssetHandle handle{ content::assets::GetHandleFromImportedPath(DEFAULT_TEXTURES_PATHS[i]) };
		DEFAULT_TEXTURES[i] = content::assets::CreateResourceFromHandle(handle);
		assert(id::IsValid(DEFAULT_TEXTURES[i]));
	}
	return true;
}

void
DisplayMaterialSurfaceProperties(const graphics::MaterialSurface& surface)
{
	if (ImGui::BeginTable("Surface Properties", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableNextRow();
		ImGui::TextUnformatted("Surface:");
		ImGui::TableNextRow();
		ui::DisplayVector4(surface.BaseColor, "Base Color");
		ImGui::TableNextRow();
		ui::DisplayFloat(surface.Metallic, "Metallic");
		ImGui::TableNextRow();
		ui::DisplayFloat(surface.Roughness, "Roughness");
		ImGui::TableNextRow();
		ui::DisplayVector3(surface.Emissive, "Emission Color");
		ImGui::TableNextRow();
		ui::DisplayFloat(surface.EmissiveIntensity, "Emission Intensity");
		ImGui::TableNextRow();
		ui::DisplayFloat(surface.AmbientOcclusion, "Ambient Occlusion");
		ImGui::EndTable();
	}
}

void
DisplayEditableMaterialSurfaceProperties(graphics::MaterialSurface& surface)
{
	if (ImGui::BeginTable("Surface Properties", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableNextRow();
		ImGui::TextUnformatted("Surface:");
		ImGui::TableNextRow();
		constexpr f32 min{ 0.f };
		constexpr f32 max{ 1.f };
		static bool colorEditOpen{ false };
		ui::DisplayLabelT("Base Color");
		ImGui::ColorEdit4("", (float*)&surface.BaseColor, ImGuiColorEditFlags_PickerHueWheel);
		ImGui::TableNextRow();
		ui::DisplayEditableFloat(&surface.Metallic, "Metallic", min, max);
		ImGui::TableNextRow();
		ui::DisplayEditableFloat(&surface.Roughness, "Roughness", min, max);
		ImGui::TableNextRow();
		ui::DisplayEditableVector3(&surface.Emissive, "Emission Color", min, max);
		ImGui::TableNextRow();
		ui::DisplayEditableFloat(&surface.EmissiveIntensity, "Emission Intensity", min, max);
		ImGui::TableNextRow();
		ui::DisplayEditableFloat(&surface.AmbientOcclusion, "Ambient Occlusion", min, max);
		ImGui::EndTable();
	}
	DisplayStandardMaterialEdit(standardMaterial);

}

void
DisplayMaterialFlags()
{
	ImGui::TextUnformatted("Flags:");
	ImGui::CheckboxFlags("Texture Repeat", &editorMaterial.Flags, graphics::MaterialFlags::TextureRepeat);
	ImGui::CheckboxFlags("No Face Culling", &editorMaterial.Flags, graphics::MaterialFlags::NoFaceCulling);
	ImGui::TextUnformatted("Blend State:");
	//ImGui::Indent();
	constexpr const char* BLEND_MODE_OPTIONS[]{ "Alpha", "Additive", "Premultiplied Alpha" };
	static s32 blendOpt{ 0 };
	if (ImGui::ListBox("Blend Mode", &blendOpt, BLEND_MODE_OPTIONS, _countof(BLEND_MODE_OPTIONS)))
	{
		editorMaterial.Flags &= !(graphics::MaterialFlags::BlendAlpha | graphics::MaterialFlags::BlendAdditive | graphics::MaterialFlags::PremultipliedAlpha);
		editorMaterial.Flags |= (1u << (graphics::MaterialFlags::BlendAlpha + (u32)blendOpt));
	}
	//ImGui::Unindent();
	ImGui::CheckboxFlags("Depth Buffer Disabled", &editorMaterial.Flags, graphics::MaterialFlags::DepthBufferDisabled);
	ImGui::Text("value: %u", editorMaterial.Flags);
}

void
ReleaseResources()
{
	//TODO: do i need to, they are deleted automatically by the creator
}

void
OpenMaterialEditor(ecs::Entity entityID, ecs::component::RenderMaterial mat)
{
	materialOwner = entityID;
	editorMaterial = {};
	
	//TODO: could also just use mat.MaterialAsset and call UpdateMaterialInitInfo();
	materialInitInfo = graphics::GetMaterialReflection(mat.MaterialID);
	editorMaterial.TextureCount = materialInitInfo.TextureCount;
	// TODO: figure out which texture is for what usage
	memcpy(editorMaterial.TextureIDs, materialInitInfo.TextureIDs, sizeof(id_t) * materialInitInfo.TextureCount);
	bool textured{ materialInitInfo.TextureCount != 0 };
	std::pair<id_t, id_t> vsps{ content::GetDefaultPsVsShadersTextured() };
	materialInitInfo.ShaderIDs[shaders::ShaderType::Vertex] = vsps.first;
	materialInitInfo.ShaderIDs[shaders::ShaderType::Pixel] = vsps.second;
	for (u32 i{ 0 }; i < TextureUsage::Count; ++i)
	{
		editorMaterial.ShaderIDs[i] = materialInitInfo.ShaderIDs[i];
	}
	editorMaterial.Surface = materialInitInfo.Surface;
	editorMaterial.TextureCount = materialInitInfo.TextureCount;
	currentMaterialAsset = mat.MaterialAsset;

	isOpen = true;
}

void 
OpenMaterialView(content::AssetHandle handle)
{
	content::AssetPtr asset{ content::assets::GetAsset(handle) };
	LoadMaterialAsset(editorMaterial, asset->ImportedFilePath);
	currentMaterialAsset = handle;
	UpdateMaterialInitInfo();
	isOpen = true;
}

void
OpenMaterialCreator(ecs::Entity entityID)
{
	materialOwner = entityID;
	OpenMaterialView(content::assets::DEFAULT_MATERIAL_UNTEXTURED_HANDLE);
}

id_t 
GetDefaultTextureID(TextureUsage::Usage usage)
{
	return DEFAULT_TEXTURES[usage];
}

id_t
GetDefaultShaderID(TextureUsage::Usage usage)
{
	return DEFAULT_TEXTURES[usage];
}

EditorMaterial
GetDefaultEditorMaterial()
{
	EditorMaterial mat{};
	mat.Type = graphics::MaterialType::Opaque;
	mat.Surface = graphics::MaterialSurface{};
	mat.Flags = 0;
	mat.Name = "Default material";
	auto psvs{ content::GetDefaultPsVsShadersTextured() };
	mat.ShaderIDs[0] = psvs.first;
	mat.ShaderIDs[1] = psvs.second;
	mat.TextureCount = TextureUsage::Count;
	for (u32 i{ 0 }; i < TextureUsage::Count; ++i) mat.TextureIDs[i] = DEFAULT_TEXTURES[i];
	return mat;
}

EditorMaterial
GetDefaultEditorMaterialUntextured()
{
	EditorMaterial mat{};
	mat.Type = graphics::MaterialType::Opaque;
	mat.Surface = graphics::MaterialSurface{};
	mat.Flags = 0;
	mat.Name = "Default material";
	auto psvs{ content::GetDefaultPsVsShaders() };
	mat.ShaderIDs[0] = psvs.first;
	mat.ShaderIDs[1] = psvs.second;
	mat.TextureCount = 0;
	return mat;
}

void
RenderMaterialEditor()
{
	if (!isOpen) return;

	ImGui::Begin("Material View", &isOpen);

	ImGui::Text("Material: %lu", currentMaterialAsset.id);
	ImGui::Text("Type: %s", MATERIAL_SURFACE_TYPE_TO_STRING[materialInitInfo.Type]);
	ImGui::NextColumn();

	DisplayTexture(TextureUsage::BaseColor, "Base Color:", "##Base");
	DisplayTexture(TextureUsage::Normal, "Normal:", "##Normal");
	DisplayTexture(TextureUsage::MetallicRoughness, "MetallicRoughness:", "##MetRou");
	DisplayTexture(TextureUsage::Emissive, "Emissive:", "##Emiss");
	DisplayTexture(TextureUsage::AmbientOcclusion, "Ambient Occlusion:", "##AO");

	DisplayEditableMaterialSurfaceProperties(editorMaterial.Surface);
	DisplayMaterialFlags();

	if (ImGui::Button("Update"))
	{
		if (currentMaterialAsset == content::assets::DEFAULT_MATERIAL_UNTEXTURED_HANDLE)
		{
			// avoid modyfying the standard material
			DuplicateCurrentMaterial();
		}
		//TODO: or edit the material texture data itself
		UpdateMaterialInitInfo();
		id_t newMaterialID{ content::CreateMaterial(materialInitInfo) };
		//TODO: formal way
		ecs::component::RenderMaterial& mat{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(materialOwner) };
		ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(materialOwner) };
		mat.MaterialID = newMaterialID;

		// update the asset
		// for now just reimport it to the same path
		content::AssetPtr asset{ content::assets::GetAsset(currentMaterialAsset) };
		if (asset)
		{
			//material::PackMaterialAsset(editorMaterial, asset->ImportedFilePath);
			//content::assets::PairAssetWithResource(currentMaterialAsset, newMaterialID, content::AssetType::Material);
			//mat.MaterialAsset = currentMaterialAsset;

			//graphics::d3d12::core::RenderItemsUpdated();

			graphics::RemoveRenderItem(mesh.RenderItemID);
			mesh.RenderItemID = graphics::AddRenderItem(materialOwner, mesh.MeshID, mat.MaterialCount, mat.MaterialID);
			//FIXME: doesnt update assets
			/*mat.MaterialAsset = content::assets::GetAssetFromResource(newMaterialID, content::AssetType::Material);
			if (!content::IsValid(mat.MaterialAsset))
			{
				std::filesystem::path 
				material::PackMaterialAsset(editorMaterial, )
			}*/
		}
		else
		{
			assert(false);
			log::Error("Load the material asset first (TODO)");
			ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(materialOwner) };
			graphics::RemoveRenderItem(mesh.RenderItemID);
			mesh.RenderItemID = graphics::AddRenderItem(materialOwner, mesh.MeshID, mat.MaterialCount, mat.MaterialID);
		}
	}
	static bool saving{ false };
	if (ImGui::Button("Save"))
	{
		content::AssetPtr mat{ content::assets::GetAsset(currentMaterialAsset) };
		if (mat)
		{
			PackMaterialAsset(editorMaterial, mat->ImportedFilePath);
			UpdateMaterialInitInfo();
		}
		else
		{
			log::Error("Save the current material first");
		}
	}
	if (ImGui::Button("Save As New"))
	{
		saving = true;
	}
	if (saving)
	{
		ImGui::InputText("Filename", nameBuffer, MAX_NAME_LENGTH);
		{
			if (ImGui::Button("Confirm"))
			{
				std::filesystem::path outPath{ project::GetResourceDirectory() / "Materials"};
				outPath.append(nameBuffer);
				outPath.replace_extension(".mat");

				for (u32 i{ 0 }; i < TextureUsage::Count; ++i)
				{
					editorMaterial.TextureIDs[i] = materialInitInfo.TextureIDs[i];
				}
				for (u32 i{ 0 }; i < TextureUsage::Count; ++i)
				{
					editorMaterial.ShaderIDs[i] = materialInitInfo.ShaderIDs[i];
				}
				editorMaterial.Surface = materialInitInfo.Surface;
				editorMaterial.TextureCount = materialInitInfo.TextureCount;

				PackMaterialAsset(editorMaterial, outPath);
				currentMaterialAsset = material::CreateMaterialAsset(outPath);
				saving = false;
			}
		}
	}
	if (ImGui::Button("Load"))
	{
		ImGui::OpenPopup("LoadMaterialPopup");
	}
	if (ImGui::BeginPopup("LoadMaterialPopup"))
	{
		matFiles.clear();
		content::ListFilesByExtensionRec(".mat", project::GetResourceDirectory(), matFiles);

		for (std::string_view matPath : matFiles)
		{
			if (ImGui::Selectable(matPath.data()))
			{
				content::AssetHandle assetHandle{ content::assets::GetHandleFromImportedPath(matPath) };
				if (assetHandle != content::INVALID_HANDLE)
				{
					//TODO: refactor
					LoadMaterialAsset(editorMaterial, matPath);
					materialInitInfo = {};
					LoadMaterialDataFromAsset(materialInitInfo, assetHandle);
					currentMaterialAsset = assetHandle;
				}
				else
				{
					log::Warn("Can't find Material assetHandle from path");
				}
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
	if (ImGui::Button("Default Material"))
	{
		editorMaterial = GetDefaultEditorMaterialUntextured();
		UpdateMaterialInitInfo();
	}
	if (ImGui::Button("Duplicate Material"))
	{
		DuplicateCurrentMaterial();
	}

	if (isBrowserOpen) RenderTextureBrowser();

	ImGui::End();
}




}