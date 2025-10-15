#pragma once
#include "CommonHeaders.h"
#include "Content/ContentUtils.h"
#include "ContentManagement.h"
#include "Content/Shaders/ShaderData.h"

namespace mofu::content {

struct LodOffset
{
	u16 Offset;
	u16 Count;
};

struct UploadedGeometryInfo
{
	id_t GeometryContentID{ id::INVALID_ID };
	u32 SubmeshCount{ 0 };
	Vec<id_t> SubmeshGpuIDs;
};

struct TextureFlags
{
	enum Flags : u32
	{
		IsHdr = 0x01,
		HasAlpha = 0x02,
		IsPremultipliedAlpha = 0x04,
		IsImportedAsNormalMap = 0x08,
		IsCubeMap = 0x10,
		IsVolumeMap = 0x20,
		IsSrgb = 0x40,
	};
};

[[nodiscard]] id_t CreateResourceFromBlob(const void* const blob, AssetType::type resourceType);
[[nodiscard]] id_t CreateResourceFromBlobWithHandle(const void* const blob, AssetType::type resourceType, AssetHandle handle);
void DestroyResource(id_t resourceId, AssetType::type resourceType);

void GetLODOffsets(const id_t* const geometryIDs, const f32* const thresholds, u32 idCount, Vec<LodOffset>& offsets);

id_t AddShaderGroup(const u8* const* shaders, u32 shaderCount, const u32* const keys);
void RemoveShaderGroup(id_t id);
shaders::CompiledShaderPtr GetShader(id_t groupID, u32 shaderKey);

//TODO: bad idea 
UploadedGeometryInfo GetLastUploadedGeometryInfo();
}