#pragma once

#include "ImGui/imgui.h"

namespace ImGui
{
	// Cubic Bezier curve widget adapted from ocornut/imgui issue #786.
	// P[0..3] stores P1.x, P1.y, P2.x, P2.y. P0=(0,0), P3=(1,1).
	int Bezier(const char* Label, float P[4]);
	float BezierValue(float Dt01, const float P[4]);
}
