#pragma once
#include "CommonHeaders.h"
#include <chrono>

#include <Windows.h>
class Timer
{
public:
	using clock = std::chrono::high_resolution_clock;

	constexpr float AvgDt() const { return _avgDt * 1e-6f; }

	void Start()
	{
		_start = clock::now();
	}

	void End()
	{
		auto dt = clock::now() - _start;
		_usAvg += ((float)std::chrono::duration_cast<std::chrono::microseconds>(dt).count() - _usAvg) / (float)_frameCounter;
		++_frameCounter;
		_avgDt = _usAvg;

		if (std::chrono::duration_cast<std::chrono::seconds>(clock::now() - _seconds).count() >= 1)
		{
			OutputDebugStringA("Avg frame (ms): ");
			OutputDebugStringA(std::to_string(_usAvg * 0.001f).c_str());
			OutputDebugStringA((" " + std::to_string(_frameCounter)).c_str());
			OutputDebugStringA("fps\n ");
			_usAvg = 0.f;
			_frameCounter = 1;
			_seconds = clock::now();
		}
	}

private:
	float _avgDt{ 0.f };
	float _usAvg{ 0.f };
	u32 _frameCounter{ 1 };
	clock::time_point _start;
	clock::time_point _seconds{ clock::now() };
};