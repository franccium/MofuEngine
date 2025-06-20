#pragma once
#include "CommonHeaders.h"
#include <limits>

#include "imgui.h"

namespace mofu::editor {

static constexpr f32 MIN_EDITABLE_U32{ 0 };
static constexpr f32 MAX_EDITABLE_U32{ 100 };
static constexpr f32 MIN_EDITABLE_FLOAT{ -1.f };
static constexpr f32 MAX_EDITABLE_FLOAT{ 1.f };
// NOTE: using numeric_limits as default for these make sliders unusable, keep the min/max values small
void DisplayEditableUint(u32* v, const char* label, u32 minVal = MIN_EDITABLE_U32, u32 maxVal = MAX_EDITABLE_U32);
void DisplayEditableFloat(f32* v, const char* label, f32 minVal = MIN_EDITABLE_FLOAT, f32 maxVal = MAX_EDITABLE_FLOAT);
void DisplayEditableVector2(v2* v, const char* label, f32 minVal = MIN_EDITABLE_FLOAT, f32 maxVal = MAX_EDITABLE_FLOAT);
void DisplayEditableVector3(v3* v, const char* label, f32 minVal = MIN_EDITABLE_FLOAT, f32 maxVal = MAX_EDITABLE_FLOAT);
void DisplayEditableVector4(v4* v, const char* label, f32 minVal = MIN_EDITABLE_FLOAT, f32 maxVal = MAX_EDITABLE_FLOAT);

void DisplayMatrix4x4(m4x4* m, const char* label);
void DisplayUint(u32 v, const char* label);
void DisplayFloat(f32 v, const char* label);
}