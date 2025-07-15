#include "NormalMapProcessing.h"
#include "Content/ResourceCreation.h"
#include "Content/ContentUtils.h"
#include "Utilities/IOStream.h"
#include <DirectXTex.h>

using namespace DirectX;

namespace mofu::content::texture {
namespace {
constexpr f32 INV_255{ 1.f / 255.f };
constexpr f32 MIN_AVG_LENGTH_THRESHOLD{ 0.7f };
constexpr f32 MAX_AVG_LENGTH_THRESHOLD{ 1.1f };
constexpr f32 MIN_AVG_Z_THRESHOLD{ 0.8f };
constexpr f32 VECTOR_LENGTH_SQ_REJECTION_THRESHOLD{ MIN_AVG_LENGTH_THRESHOLD * MIN_AVG_LENGTH_THRESHOLD };
constexpr f32 REJECTION_RATIO_THRESHOLD{ 0.33f };

using Sampler = Color(*)(const u8* const);
Color
SamplePixelRGB(const u8* const px)
{
	Color c{ (f32)px[0], (f32)px[1], (f32)px[2], (f32)px[3] };
	return c * INV_255;
}

Color
SamplePixelBGR(const u8* const px)
{
	Color c{ (f32)px[2], (f32)px[1], (f32)px[0], (f32)px[3] };
	return c * INV_255;
}

s32
EvaluateColor(Color c)
{
	if (c.IsBlack() || c.IsTransparent()) return 0;

	v3 v{ c.r * 2.f - 1.f, c.g * 2.f - 1.f, c.b * 2.f - 1.f };
	const f32 vLengthSq{ v.x * v.x + v.y * v.y + v.z * v.z };
	return (v.z < 0.f || vLengthSq < VECTOR_LENGTH_SQ_REJECTION_THRESHOLD) ? -1 : 1;
}

bool
IsImageNormalMap(const Image* const image, Sampler sample)
{
	constexpr u32 SAMPLE_COUNT{ 4096 };
	const size_t imageSize{ image->slicePitch };
	const size_t sampleInterval{ std::max(imageSize / SAMPLE_COUNT, size_t(4)) };
	// the minimum amount of samples that must be accepted is 25% of all samples
	const u32 minAcceptedSampleCount{ std::max((u32)(imageSize / sampleInterval) >> 2, (u32)1) };
	const u8* const pixels{ image->pixels };

	u32 rejectedSamples{ 0 };
	u32 acceptedSamples{ 0 };
	Color avgColor{};

	size_t offset{ sampleInterval };
	while (offset < imageSize)
	{
		const Color c{ sample(&pixels[offset]) };
		const s32 result{ EvaluateColor(c) };
		if (result < 0)
		{
			++rejectedSamples;
		}
		else
		{
			++acceptedSamples;
			avgColor += c;
		}
		offset += sampleInterval;
	}

	if (acceptedSamples >= minAcceptedSampleCount)
	{
		const f32 rejectedRatio{ (f32)rejectedSamples / (f32)acceptedSamples };
		if (rejectedRatio > REJECTION_RATIO_THRESHOLD) return false;
		
		avgColor /= (f32)acceptedSamples;
		v3 v{ avgColor.r * 2.f - 1.f, avgColor.g * 2.f - 1.f, avgColor.b * 2.f - 1.f };
		const f32 avgLength{ sqrt(v.x * v.x + v.y * v.y + v.z * v.z) };
		const f32 avgNormalizedZ{ v.z / avgLength };

		return avgLength >= MIN_AVG_LENGTH_THRESHOLD && avgLength <= MAX_AVG_LENGTH_THRESHOLD && avgNormalizedZ >= MIN_AVG_Z_THRESHOLD;
	}

	return false;
}

} // anonymous namespace

bool
IsNormalMap(const Image* const image)
{
	const DXGI_FORMAT imageFormat{ image->format };
	if (BitsPerPixel(imageFormat) != 32 || BitsPerColor(imageFormat) != 8) return false;

	return IsImageNormalMap(image, IsBGR(imageFormat) ? SamplePixelBGR : SamplePixelRGB);
}
}