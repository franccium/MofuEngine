#include "FontRenderer.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "External/text/stb_truetype.h"
#include "Content/ContentUtils.h"
#include "Graphics/D3D12/D3D12Content/D3D12Texture.h"
#include "Utilities/Logger.h"

namespace mofu::graphics::d3d12::debug::font {
namespace {
id_t _defaultFontTexID{ U32_INVALID_ID };
v2 _defaultFontTexSize{};
std::string _defaultFontName{};
}

bool
FontRenderer::Initialize(const char* fontName, u32 charHeight)
{
	FontName = fontName;
	CharHeight = charHeight;
	HorizontalTexelCount = 64;
	VerticalTexelCount = 64;
	constexpr u8 charHorizontalSpacing{ 2 };
	constexpr u8 charVerticalSpacing{ 2 };

	std::filesystem::path fontPath{ FONTS_PATH };
	fontPath.append(fontName);
	assert(std::filesystem::exists(fontPath));
	if (!std::filesystem::exists(fontPath)) return false;
	
	std::unique_ptr<u8[]> fontData;
	u64 fontDataSize;
	mofu::content::ReadFileToByteBuffer(fontPath, fontData, fontDataSize);

	stbtt_fontinfo font;
	if (!stbtt_InitFont(&font, fontData.get(), stbtt_GetFontOffsetForIndex(fontData.get(), 0))) return false;

	f32 scale{ stbtt_ScaleForPixelHeight(&font, (f32)CharHeight) };
	s32 ascent;
	stbtt_GetFontVMetrics(&font, &ascent, nullptr, nullptr);
	s32 baseline = s32(ascent * scale);

	content::texture::GeneratedTextureData textureData{};
	textureData.ArraySize = 1;
	textureData.MipLevels = 1;
	textureData.Format = DXGI_FORMAT_R8_UNORM;
	const u8 formatBpp{ 1 };
	textureData.Stride = formatBpp;
	textureData.Flags = 0;

	bool foundTextureSize{ false };
	while (!foundTextureSize && HorizontalTexelCount > 0 && VerticalTexelCount > 0)
	{
		foundTextureSize = true;
		textureData.Width = HorizontalTexelCount;
		textureData.Height = VerticalTexelCount;
		const u32 textureRowPitch{ (u32)math::AlignUp<4>(HorizontalTexelCount * textureData.Stride) };
		textureData.SubresourceSize = textureRowPitch * VerticalTexelCount;
		textureData.SubresourceData = new u8[textureData.SubresourceSize];
		memset(textureData.SubresourceData, 0, textureData.SubresourceSize);

		s32 x{ 0 };
		s32 y{ 0 };
		for (u8 c{ FIRST_PRINTABLE_CHAR + 1 }; c < LAST_PRINTABLE_CHAR; ++c)
		{
			const u8 charIndex{ u8(c - FIRST_PRINTABLE_CHAR) };

			s32 w, h, xoff, yoff;
			unsigned char* bitmap{ stbtt_GetCodepointBitmap(&font, 0, scale, c, &w, &h, &xoff, &yoff) };
			yoff += baseline;

			// check for room in the line
			if (s32(x + xoff + w + charHorizontalSpacing) > HorizontalTexelCount)
			{
				// go to the next line
				x = 0;
				y += CharHeight + charVerticalSpacing;

				if (y + CharHeight + charVerticalSpacing > VerticalTexelCount)
				{
					// try a larger surface if the character didn't fit
					if (HorizontalTexelCount < 2 * VerticalTexelCount)
						HorizontalTexelCount <<= 1;
					else
						VerticalTexelCount <<= 1;

					delete[] textureData.SubresourceData;
					stbtt_FreeBitmap(bitmap, nullptr);
					foundTextureSize = false;
					break;
				}
			}
			assert(x >= 0 && x <= 0xFFFF);
			assert(y >= 0 && y <= 0xFFFF);
			assert(w <= 0xFF);

			StartU[charIndex] = (u16)x;
			StartV[charIndex] = (u16)y;
			CharWidths[charIndex] = (u8)w + 1u;

			for (s32 y2{ 0 }; y2 < h; ++y2)
			{
				u8* const src{ bitmap + y2 * w };
				u8* const dst{ &textureData.SubresourceData[(y + yoff + y2) * textureRowPitch] + x + xoff };
				memcpy(dst, src, w);
			}
			stbtt_FreeBitmap(bitmap, nullptr);

			x += w + charHorizontalSpacing;
		}
	}
	if (!foundTextureSize) return false;

	for (u8 cidx1{ 0 }; cidx1 < PRINTABLE_CHAR_COUNT; ++cidx1)
	{
		for (u8 cidx2{ 0 }; cidx2 < PRINTABLE_CHAR_COUNT; ++cidx2)
		{
			u8 c1{ u8(FIRST_PRINTABLE_CHAR + cidx1) };
			u8 c2{ u8(FIRST_PRINTABLE_CHAR + cidx2) };
			s32 advance;
			stbtt_GetCodepointHMetrics(&font, c1, &advance, nullptr);
			s32 spacing{ math::Clamp(s32(scale * (advance + stbtt_GetCodepointKernAdvance(&font, c1, c2))), 0, 0xFF) };
			Spacing[cidx1][cidx2] = (u8)spacing;
		}
	}

	TextureID = content::texture::CreateTextureFromGeneratedData(textureData);
	_defaultFontTexID = TextureID;
	_defaultFontTexSize = { (f32)textureData.Width, (f32)textureData.Height };
	_defaultFontName = fontName;
	assert(id::IsValid(TextureID));

	UCorrection = 1.f / HorizontalTexelCount;
	VCorrection = 1.f / VerticalTexelCount;

	return true;
}

void
FontRenderer::RenderText(const D3D12FrameInfo& frameInfo, D3D12_GPU_VIRTUAL_ADDRESS constants,
	JPH::Mat44Arg transform, std::string_view text, JPH::ColorArg color) const
{
	u32 charCount{ 0 };
	//TODO: make this loop only contain valid characters, maybe span, maybe dont allow anything other than valid; have to handle \n though
	for (u32 i{ 0 }; i < text.size(); ++i)
	{
		u8 c{ (u8)text[i] };
		if (c > FIRST_PRINTABLE_CHAR && c < LAST_PRINTABLE_CHAR) charCount++;
	}
	if (charCount == 0) return;

	const u32 vertexCount{ charCount * 4 };
	const u32 indexCount{ charCount * 6 };

	TempStructuredBuffer vertexBuffer{vertexCount, sizeof(FontVertex), false };
	TempStructuredBuffer indexBuffer{indexCount, sizeof(u32), false };

	FontVertex* fontVertices{ (FontVertex*)vertexBuffer.CPUAddress };
	u32* const fontIndices{ (u32* const)indexBuffer.CPUAddress };

	u32* currentIndex{ fontIndices };
	u32 currentVertex{ 0 };

	f32 xPos{ 0.f };
	f32 yPos{ -1.f };

	for (u32 i{ 0 }; i < text.size(); ++i)
	{
		u8 c{ (u8)text[i] };
		if (c > FIRST_PRINTABLE_CHAR && c < LAST_PRINTABLE_CHAR)
		{
			currentIndex[0] = currentVertex + 0;
			currentIndex[1] = currentVertex + 1;
			currentIndex[2] = currentVertex + 2;
			currentIndex[3] = currentVertex + 2;
			currentIndex[4] = currentVertex + 1;
			currentIndex[5] = currentVertex + 3;

			currentVertex += 4;
			currentIndex += 6;

			const u8 charIndex{ u8(c - FIRST_PRINTABLE_CHAR) };

			const JPH::Float2 uvStart{ UCorrection * StartU[charIndex], VCorrection * StartV[charIndex] };
			const JPH::Float2 uvEnd{ UCorrection * (StartU[charIndex] + CharWidths[charIndex]), VCorrection * (StartV[charIndex] + CharHeight) };
			const JPH::Float2 xyEnd{ xPos + (f32)CharWidths[charIndex] / CharHeight, yPos + 1.0f };

			(transform * JPH::Vec3(xPos, yPos, 0.f)).StoreFloat3(&fontVertices->Position);
			fontVertices->Color = color;
			fontVertices->UV = JPH::Float2(uvStart.x, uvEnd.y);
			++fontVertices;
			(transform * JPH::Vec3(xPos, xyEnd.y, 0.f)).StoreFloat3(&fontVertices->Position);
			fontVertices->Color = color;
			fontVertices->UV = uvStart;
			++fontVertices;
			(transform * JPH::Vec3(xyEnd.x, yPos, 0.f)).StoreFloat3(&fontVertices->Position);
			fontVertices->Color = color;
			fontVertices->UV = uvEnd;
			++fontVertices;
			(transform * JPH::Vec3(xyEnd.x, xyEnd.y, 0.f)).StoreFloat3(&fontVertices->Position);
			fontVertices->Color = color;
			fontVertices->UV = JPH::Float2(uvEnd.x, uvStart.y);
			++fontVertices;
		}

		if (c == '\n')
		{
			xPos = 0.f;
			yPos -= 1.f;
		}
		else if (i + 1 < text.size())
		{
			u8 c1{ u8(c - FIRST_PRINTABLE_CHAR) };
			s16 c2{ s16((u8)text[i + 1] - FIRST_PRINTABLE_CHAR) };
			if (c2 >= 0 && c2 < PRINTABLE_CHAR_COUNT)
			{
				xPos += f32(Spacing[c1][c2]) / CharHeight;
			}
		}
	}

	assert(currentVertex == vertexCount);
	assert(currentIndex == fontIndices + indexCount);

	DXGraphicsCommandList* const cmdList{ core::GraphicsCommandList() };
	D3D12_VERTEX_BUFFER_VIEW vb{};
	vb.BufferLocation = vertexBuffer.GPUAddress;
	vb.SizeInBytes = sizeof(FontVertex) * vertexCount;
	vb.StrideInBytes = sizeof(FontVertex);
	D3D12_INDEX_BUFFER_VIEW ib{};
	ib.BufferLocation = indexBuffer.GPUAddress;
	ib.Format = DXGI_FORMAT_R32_UINT;
	ib.SizeInBytes = indexCount * sizeof(u32);
	cmdList->IASetVertexBuffers(0, 1, &vb);
	cmdList->IASetIndexBuffer(&ib);
	cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}

id_t GetFontTextureID()
{
	return _defaultFontTexID;
}
v2 GetFontTextureSize()
{
	return _defaultFontTexSize;
}
std::string GetFontName()
{
	return _defaultFontName;
}
}