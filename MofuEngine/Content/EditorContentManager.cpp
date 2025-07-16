#include "EditorContentManager.h"
#include <hash_map>

namespace mofu::content::editor {
namespace {

std::hash_map<AssetHandle, Asset> assetRegistry{};
std::hash_map<AssetHandle, 

} // anonymous namespace



void
ReadAssetFile(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, AssetType::type type)
{
}

void
ReadAssetFileNoVersion(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, AssetType::type type)
{
}

}
