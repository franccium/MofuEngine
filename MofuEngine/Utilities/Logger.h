#pragma once
#include "CommonHeaders.h"  
#include "imgui.h"

namespace mofu::log {
enum LogSeverityLevel
{
	LogInfo = 0,
	LogWarning,
	LogError,
	Count
};

void Initialize();
void Shutdown();
void Clear();
void Info(const char* fmt, ...) IM_FMTARGS(2);
void Warn(const char* fmt, ...) IM_FMTARGS(2);
void Error(const char* fmt, ...) IM_FMTARGS(2);
void Draw(const char* title, bool* p_open = NULL);
void AddTestLogs();
}