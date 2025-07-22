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
id_t DEFAULT_SHADERS[graphics::ShaderType::Count]{ id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID };
id_t usedTextures[TextureUsage::Count];

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
			content::ListFilesByExtension(".tex", project::GetResourceDirectory(), texFiles);
			ImGui::OpenPopup("SelectTexturePopup");
			isBrowserOpen = true;
			textureBeingChanged = texUse;
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
			content::ListFilesByExtension(".tex", project::GetResourceDirectory(), texFiles);
			ImGui::OpenPopup("SelectTexturePopup");
			isBrowserOpen = true;
			textureBeingChanged = texUse;
		}
		ImGui::PopID();
	}

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		texture::OpenTextureView(editorMaterial.TextureIDs[texUse]);
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
	memcpy(materialInitInfo.ShaderIDs, editorMaterial.ShaderIDs, graphics::ShaderType::Count * sizeof(id_t));
	materialInitInfo.TextureIDs = new id_t[editorMaterial.TextureCount];
	memcpy(materialInitInfo.TextureIDs, editorMaterial.TextureIDs, editorMaterial.TextureCount * sizeof(id_t));
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
					materialInitInfo.TextureIDs[textureBeingChanged] = content::assets::CreateResourceFromHandle(assetHandle);
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
		DisplayVector4(surface.BaseColor, "Base Color");
		ImGui::TableNextRow();
		DisplayFloat(surface.Metallic, "Metallic");
		ImGui::TableNextRow();
		DisplayFloat(surface.Roughness, "Roughness");
		ImGui::TableNextRow();
		DisplayVector3(surface.Emissive, "Emission Color");
		ImGui::TableNextRow();
		DisplayFloat(surface.EmissiveIntensity, "Emission Intensity");
		ImGui::TableNextRow();
		DisplayFloat(surface.AmbientOcclusion, "Ambient Occlusion");
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
		DisplayEditableVector4(&surface.BaseColor, "Base Color", min, max);
		ImGui::TableNextRow();
		DisplayEditableFloat(&surface.Metallic, "Metallic", min, max);
		ImGui::TableNextRow();
		DisplayEditableFloat(&surface.Roughness, "Roughness", min, max);
		ImGui::TableNextRow();
		DisplayEditableVector3(&surface.Emissive, "Emission Color", min, max);
		ImGui::TableNextRow();
		DisplayEditableFloat(&surface.EmissiveIntensity, "Emission Intensity", min, max);
		ImGui::TableNextRow();
		DisplayEditableFloat(&surface.AmbientOcclusion, "Ambient Occlusion", min, max);
		ImGui::EndTable();
	}
}

void
OpenMaterialEditor(ecs::Entity entityID, ecs::component::RenderMaterial mat)
{
	materialOwner = entityID;
	editorMaterial = {};
	materialInitInfo = graphics::GetMaterialReflection(mat.MaterialIDs[0]);
	editorMaterial.TextureCount = materialInitInfo.TextureCount;
	// TODO: figure out which texture is for what usage
	memcpy(editorMaterial.TextureIDs, materialInitInfo.TextureIDs, sizeof(id_t) * materialInitInfo.TextureCount);
	bool textured{ materialInitInfo.TextureCount != 0 };
	std::pair<id_t, id_t> vsps{ content::GetDefaultPsVsShadersTextured() };
	materialInitInfo.ShaderIDs[graphics::ShaderType::Vertex] = vsps.first;
	materialInitInfo.ShaderIDs[graphics::ShaderType::Pixel] = vsps.second;
	for (u32 i{ 0 }; i < TextureUsage::Count; ++i)
	{
		editorMaterial.ShaderIDs[i] = materialInitInfo.ShaderIDs[i];
	}
	editorMaterial.Surface = materialInitInfo.Surface;
	editorMaterial.TextureCount = materialInitInfo.TextureCount;
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

graphics::MaterialInitInfo
GetDefaultMaterialInitInfo()
{
	graphics::MaterialInitInfo info{};
	info.Type = graphics::MaterialType::Opaque;
	info.Surface = graphics::MaterialSurface{};
	info.TextureCount = TextureUsage::Count;
	info.TextureIDs = &DEFAULT_TEXTURES[0];
	auto psvs{ content::GetDefaultPsVsShadersTextured() };
	info.ShaderIDs[0] = psvs.first;
	info.ShaderIDs[1] = psvs.second;
	return info;
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

	if (ImGui::Button("Update"))
	{
		//TODO: or edit the material texture data itself
		id_t newMaterialID{ content::CreateMaterial(materialInitInfo) };
		//TODO: formal way
		ecs::component::RenderMaterial& mat{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(materialOwner) };
		ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(materialOwner) };
		mat.MaterialIDs[0] = newMaterialID;

		// update the asset
		// for now just reimport it to the same path
		content::AssetPtr asset{ content::assets::GetAsset(currentMaterialAsset) };
		if (asset)
		{
			material::PackMaterialAsset(editorMaterial, asset->ImportedFilePath);
			content::assets::PairAssetWithResource(currentMaterialAsset, newMaterialID, content::AssetType::Material);
			mat.MaterialAsset = currentMaterialAsset;

			//graphics::d3d12::core::RenderItemsUpdated();

			graphics::RemoveRenderItem(id::Index(materialOwner) - 1);
			mesh.RenderItemID = graphics::AddRenderItem(materialOwner, mesh.MeshID, mat.MaterialCount, mat.MaterialIDs);
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
			log::Error("Load the material asset first (TODO)");
			graphics::RemoveRenderItem(id::Index(materialOwner) - 1);
			mesh.RenderItemID = graphics::AddRenderItem(materialOwner, mesh.MeshID, mat.MaterialCount, mat.MaterialIDs);
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
		content::AssetPtr mat{ content::assets::GetAsset(currentMaterialAsset) };
		if (mat)
		{
			std::filesystem::path duplicatePath{ content::CreateUniqueDuplicateName(mat->ImportedFilePath) };
			PackMaterialAsset(editorMaterial, duplicatePath);
			currentMaterialAsset = CreateMaterialAsset(duplicatePath);
			UpdateMaterialInitInfo();
		}
		else
		{
			log::Error("Save the current material first");
		}
	}

	if (isBrowserOpen) RenderTextureBrowser();

	ImGui::End();
}


}