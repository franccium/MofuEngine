#pragma once
#include "CommonHeaders.h"
#include "Platform/Window.h"

// A high level renderer with function pointers to the set platform's implementation
namespace mofu::graphics {
struct FrameInfo
{
    f32 lastFrameTime{ 16.7f };
    f32 averageFrameTime{ 0.f };
};

DEFINE_TYPED_ID(surface_id)
class Surface
{
public:
    constexpr explicit Surface(surface_id id) : _id{ id } {}
    constexpr Surface() = default;
    constexpr surface_id GetID() const { return _id; }
    constexpr bool IsValid() const { return id::is_valid(_id); }

    void Resize(u32 width, u32 height) const;
    u32 Width() const;
    u32 Height() const;
    void Render(FrameInfo info) const;

private:
    surface_id _id{ id::INVALID_ID };
};

struct RenderSurface
{
    platform::Window window{};
    Surface surface;
};

struct ShaderFlags
{
	enum flags : u32
	{
		None = 0x00,
		Vertex = 0x01,
		Hull = 0x02,
		Domain = 0x04,
		Geometry = 0x08,
		Pixel = 0x10,
		Compute = 0x20,
		Amplification = 0x40,
		Mesh = 0x80
	};
};

struct ShaderType
{
	enum type : u32
	{
		Vertex = 0,
		Hull,
		Domain,
		Geometry,
		Pixel,
		Compute,
		Amplification,
		Mesh,

		Count
	};
};

enum class GraphicsPlatform : u32
{
	Direct3D12 = 0,
};

bool Initialize(GraphicsPlatform platform);
void Shutdown();

Surface CreateSurface(platform::Window window);
void RemoveSurface(surface_id id);

const char* GetEngineShadersPath();


}