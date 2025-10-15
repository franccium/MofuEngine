#pragma once
#include "../D3D12CommonHeaders.h"
#include "Graphics/Renderer.h"
#include "D3D12Texture.h"

namespace mofu::graphics::d3d12::content::material {
struct MaterialsCache
{
	ID3D12RootSignature** const RootSignatures;
	MaterialType::type* const MaterialTypes;
	u32** const DescriptorIndices;
	u32* const TextureCounts;
	MaterialSurface** const MaterialSurfaces;
};

id_t CreateRootSignature(MaterialType::type materialType, ShaderFlags::Flags shaderFlags, MaterialFlags::Flags materialFlags);

class D3D12MaterialStream
{
public:
    DISABLE_COPY_AND_MOVE(D3D12MaterialStream);
    explicit D3D12MaterialStream(u8* const materialBuffer) : _buffer{ materialBuffer }
    {
        Initialize();
    }

    explicit D3D12MaterialStream(std::unique_ptr<u8[]>& materialBuffer, MaterialInitInfo info)
    {
        assert(!materialBuffer);
        u32 shaderCount{ 0 };
        u32 shaderFlags{ 0 };
        u32 materialFlags{ 0 };
        for (u32 i{ 0 }; i < mofu::shaders::ShaderType::Count; ++i)
        {
            if (id::IsValid(info.ShaderIDs[i]))
            {
                ++shaderCount;
                shaderFlags |= (1 << i);
            }
        }
        assert(shaderCount && shaderFlags);

        const u32 bufferSize
        {
            sizeof(MaterialType::type) +
            sizeof(ShaderFlags::Flags) +
            sizeof(MaterialFlags::Flags) +
            sizeof(id_t) + // root signature id
            sizeof(u32) + // texture count
            sizeof(MaterialSurface) +
            sizeof(id_t) * shaderCount + // shader ids
            (sizeof(id_t) + sizeof(u32)) * info.TextureCount // texture ids and descriptor indices
        };

        materialBuffer = std::make_unique<u8[]>(bufferSize);
        _buffer = materialBuffer.get();

        u8* const buffer{ _buffer };
        *(MaterialType::type*)buffer = info.Type;
        *(ShaderFlags::Flags*)&buffer[SHADER_FLAGS_INDEX] = (ShaderFlags::Flags)shaderFlags;
        *(MaterialFlags::Flags*)&buffer[MATERIAL_FLAGS_INDEX] = (MaterialFlags::Flags)materialFlags;
        *(id_t*)&buffer[ROOT_SIGNATURE_INDEX] = CreateRootSignature(info.Type, (ShaderFlags::Flags)shaderFlags, (MaterialFlags::Flags)materialFlags);
        *(u32*)&buffer[TEXTURE_COUNT_INDEX] = info.TextureCount;
        *(MaterialSurface*)&buffer[MATERIAL_SURFACE_INDEX] = info.Surface;

        Initialize();
        if (info.TextureCount)
        {
            for (u32 i{ 0 }; i < info.TextureCount; ++i) assert(id::IsValid(info.TextureIDs[i]));

            memcpy(_textureIDs, info.TextureIDs, info.TextureCount * sizeof(id_t));
            texture::GetDescriptorIndices(_textureIDs, info.TextureCount, _descriptorIndices);
        }

        u32 shaderIndex{ 0 };
        for (u32 i{ 0 }; i < mofu::shaders::ShaderType::Count; ++i)
        {
            if (id::IsValid(info.ShaderIDs[i]))
            {
                _shaderIDs[shaderIndex] = info.ShaderIDs[i];
                ++shaderIndex;
            }
        }
        assert(shaderIndex == (u32)_mm_popcnt_u32(_shaderFlags));
    }

    [[nodiscard]] constexpr id_t* ShaderIDs() const { return _shaderIDs; }
    [[nodiscard]] constexpr id_t RootSignatureID() const { return _rootSignatureID; }
    [[nodiscard]] constexpr u32 TextureCount() const { return _textureCount; }
    [[nodiscard]] constexpr MaterialSurface* Surface() const { return _materialSurface; }
    [[nodiscard]] constexpr id_t* TextureIDs() const { return _textureIDs; }
    [[nodiscard]] constexpr u32* DescriptorIndices() const { return _descriptorIndices; }
    [[nodiscard]] constexpr MaterialType::type MaterialType() const { return _materialType; }
    [[nodiscard]] constexpr ShaderFlags::Flags ShaderFlags() const { return _shaderFlags; }
    [[nodiscard]] constexpr MaterialFlags::Flags MaterialFlags() const { return _materialFlags; }

    constexpr static u32 SHADER_FLAGS_INDEX{ sizeof(MaterialType::type) };
    constexpr static u32 MATERIAL_FLAGS_INDEX{ SHADER_FLAGS_INDEX + sizeof(ShaderFlags::Flags) };
    constexpr static u32 ROOT_SIGNATURE_INDEX{ MATERIAL_FLAGS_INDEX + sizeof(MaterialFlags::Flags) };
    constexpr static u32 TEXTURE_COUNT_INDEX{ ROOT_SIGNATURE_INDEX + sizeof(id_t) };
    constexpr static u32 MATERIAL_SURFACE_INDEX{ TEXTURE_COUNT_INDEX + sizeof(u32) };

private:
    void Initialize()
    {
        assert(_buffer);
        u8* const buffer{ _buffer };

        _materialType = *(MaterialType::type*)buffer;
        _shaderFlags = *(ShaderFlags::Flags*)&buffer[SHADER_FLAGS_INDEX];
        _materialFlags = *(MaterialFlags::Flags*)&buffer[MATERIAL_FLAGS_INDEX];
        _rootSignatureID = *(id_t*)&buffer[ROOT_SIGNATURE_INDEX];
        _textureCount = *(u32*)&buffer[TEXTURE_COUNT_INDEX];
        _materialSurface = (MaterialSurface*)&buffer[MATERIAL_SURFACE_INDEX];

        _shaderIDs = (id_t*)&buffer[MATERIAL_SURFACE_INDEX + sizeof(MaterialSurface)];
        _textureIDs = _textureCount ? &_shaderIDs[_mm_popcnt_u32(_shaderFlags)] : nullptr;
        _descriptorIndices = _textureCount ? (u32*)&_textureIDs[_textureCount] : nullptr;
    }

    u8* _buffer;
    MaterialType::type _materialType;
    ShaderFlags::Flags _shaderFlags;
    MaterialFlags::Flags _materialFlags;
    id_t _rootSignatureID;
    u32 _textureCount;
    MaterialSurface* _materialSurface;
    id_t* _shaderIDs;
    id_t* _textureIDs;
    u32* _descriptorIndices;
};

void GetMaterials(const id_t* const materialIds, u32 materialCount, const MaterialsCache& cache, u32& outDescriptorIndexCount);
id_t AddMaterial(const MaterialInitInfo& info);
void RemoveMaterial(id_t id);
// NOTE: used by the editor, might not want that here
MaterialInitInfo GetMaterialReflection(id_t materialID);
}