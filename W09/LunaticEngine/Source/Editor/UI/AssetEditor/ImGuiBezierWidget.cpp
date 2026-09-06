#include "Editor/UI/AssetEditor/ImGuiBezierWidget.h"

#include "ImGui/imgui_internal.h"
#include <algorithm>
#include <cstdio>

namespace ImGui
{
	static ImVec2 AddVec2(const ImVec2& A, const ImVec2& B) { return ImVec2(A.x + B.x, A.y + B.y); }
	static ImVec2 SubVec2(const ImVec2& A, const ImVec2& B) { return ImVec2(A.x - B.x, A.y - B.y); }
	static ImVec2 MulVec2(const ImVec2& A, const ImVec2& B) { return ImVec2(A.x * B.x, A.y * B.y); }
	static ImVec2 MulVec2(const ImVec2& A, float Scale) { return ImVec2(A.x * Scale, A.y * Scale); }

	template<int Steps>
	static void BezierTable(const ImVec2 P[4], ImVec2 Results[Steps + 1])
	{
		for (int Step = 0; Step <= Steps; ++Step)
		{
			const float T = static_cast<float>(Step) / static_cast<float>(Steps);
			const float U = 1.0f - T;
			const float W0 = U * U * U;
			const float W1 = 3.0f * U * U * T;
			const float W2 = 3.0f * U * T * T;
			const float W3 = T * T * T;
			Results[Step] = ImVec2(
				W0 * P[0].x + W1 * P[1].x + W2 * P[2].x + W3 * P[3].x,
				W0 * P[0].y + W1 * P[1].y + W2 * P[2].y + W3 * P[3].y);
		}
	}

	float BezierValue(float Dt01, const float P[4])
	{
		enum { Steps = 256 };
		const float TargetX = (std::max)(0.0f, (std::min)(Dt01, 1.0f));
		ImVec2 Q[4] = { ImVec2(0, 0), ImVec2(P[0], P[1]), ImVec2(P[2], P[3]), ImVec2(1, 1) };
		ImVec2 Results[Steps + 1];
		BezierTable<Steps>(Q, Results);

		for (int Index = 1; Index <= Steps; ++Index)
		{
			if (Results[Index].x >= TargetX)
			{
				const ImVec2& A = Results[Index - 1];
				const ImVec2& B = Results[Index];
				const float Range = B.x - A.x;
				const float Alpha = Range > 1e-6f ? (TargetX - A.x) / Range : 0.0f;
				return A.y + (B.y - A.y) * Alpha;
			}
		}
		return Results[Steps].y;
	}

	int Bezier(const char* Label, float P[4])
	{
		enum { Smoothness = 64 };
		constexpr float CurveWidth = 3.0f;
		constexpr float LineWidth = 1.0f;
		constexpr float GrabRadius = 6.0f;
		constexpr float GrabBorder = 2.0f;

		const ImGuiStyle& Style = GetStyle();
		ImDrawList* DrawList = GetWindowDrawList();
		ImGuiWindow* Window = GetCurrentWindow();
		if (Window->SkipItems)
		{
			return false;
		}

		PushID(Label);
		int Changed = 0;
		bool Hovered = IsItemActive() || IsItemHovered();
		Dummy(ImVec2(0.0f, 3.0f));

		const float Avail = GetContentRegionAvail().x;
		const float Dim = (std::min)(Avail, 180.0f);
		const ImVec2 Canvas(Dim, Dim);
		const ImRect Bb(Window->DC.CursorPos, AddVec2(Window->DC.CursorPos, Canvas));
		ItemSize(Bb);
		if (!ItemAdd(Bb, Window->GetID("##BezierCanvas")))
		{
			PopID();
			return Changed;
		}

		const ImGuiID Id = Window->GetID("##BezierCanvas");
		(void)Id;
		Hovered = Hovered || IsMouseHoveringRect(Bb.Min, Bb.Max);
		RenderFrame(Bb.Min, Bb.Max, GetColorU32(ImGuiCol_FrameBg, 1.0f), true, Style.FrameRounding);

		for (int I = 0; I <= 4; ++I)
		{
			const float X = Bb.Min.x + Canvas.x * (static_cast<float>(I) / 4.0f);
			const float Y = Bb.Min.y + Canvas.y * (static_cast<float>(I) / 4.0f);
			DrawList->AddLine(ImVec2(X, Bb.Min.y), ImVec2(X, Bb.Max.y), GetColorU32(ImGuiCol_TextDisabled));
			DrawList->AddLine(ImVec2(Bb.Min.x, Y), ImVec2(Bb.Max.x, Y), GetColorU32(ImGuiCol_TextDisabled));
		}

		ImVec2 Q[4] = { ImVec2(0, 0), ImVec2(P[0], P[1]), ImVec2(P[2], P[3]), ImVec2(1, 1) };
		ImVec2 Results[Smoothness + 1];
		BezierTable<Smoothness>(Q, Results);

		char Buffer[128];
		std::snprintf(Buffer, sizeof(Buffer), "0##BezierHandle");
		for (int I = 0; I < 2; ++I)
		{
			ImVec2 Pos = AddVec2(MulVec2(ImVec2(P[I * 2 + 0], 1.0f - P[I * 2 + 1]), SubVec2(Bb.Max, Bb.Min)), Bb.Min);
			SetCursorScreenPos(SubVec2(Pos, ImVec2(GrabRadius, GrabRadius)));
			Buffer[0] = static_cast<char>('0' + I);
			InvisibleButton(Buffer, ImVec2(2.0f * GrabRadius, 2.0f * GrabRadius));
			if (IsItemActive() || IsItemHovered())
			{
				SetTooltip("(%4.3f, %4.3f)", P[I * 2 + 0], P[I * 2 + 1]);
			}
			if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left))
			{
				P[I * 2 + 0] += GetIO().MouseDelta.x / Canvas.x;
				P[I * 2 + 1] -= GetIO().MouseDelta.y / Canvas.y;
				P[I * 2 + 0] = (std::max)(-1.0f, (std::min)(P[I * 2 + 0], 2.0f));
				P[I * 2 + 1] = (std::max)(-1.0f, (std::min)(P[I * 2 + 1], 2.0f));
				Changed = true;
			}
		}

		if (Hovered || Changed)
		{
			DrawList->PushClipRect(Bb.Min, Bb.Max, true);
		}

		const ImColor CurveColor(GetStyle().Colors[ImGuiCol_PlotLines]);
		for (int I = 0; I < Smoothness; ++I)
		{
			ImVec2 A(Results[I].x, 1.0f - Results[I].y);
			ImVec2 B(Results[I + 1].x, 1.0f - Results[I + 1].y);
			A = AddVec2(MulVec2(A, SubVec2(Bb.Max, Bb.Min)), Bb.Min);
			B = AddVec2(MulVec2(B, SubVec2(Bb.Max, Bb.Min)), Bb.Min);
			DrawList->AddLine(A, B, CurveColor, CurveWidth);
		}

		const ImVec4 White(GetStyle().Colors[ImGuiCol_Text]);
		const ImVec4 PointA(1.00f, 0.00f, 0.75f, 1.0f);
		const ImVec4 PointB(0.00f, 0.75f, 1.00f, 1.0f);
		ImVec2 P1 = AddVec2(MulVec2(ImVec2(P[0], 1.0f - P[1]), SubVec2(Bb.Max, Bb.Min)), Bb.Min);
		ImVec2 P2 = AddVec2(MulVec2(ImVec2(P[2], 1.0f - P[3]), SubVec2(Bb.Max, Bb.Min)), Bb.Min);
		DrawList->AddLine(ImVec2(Bb.Min.x, Bb.Max.y), P1, ImColor(White), LineWidth);
		DrawList->AddLine(ImVec2(Bb.Max.x, Bb.Min.y), P2, ImColor(White), LineWidth);
		DrawList->AddCircleFilled(P1, GrabRadius, ImColor(White));
		DrawList->AddCircleFilled(P1, GrabRadius - GrabBorder, ImColor(PointA));
		DrawList->AddCircleFilled(P2, GrabRadius, ImColor(White));
		DrawList->AddCircleFilled(P2, GrabRadius - GrabBorder, ImColor(PointB));

		if (Hovered || Changed)
		{
			DrawList->PopClipRect();
		}

		SetCursorScreenPos(ImVec2(Bb.Min.x, Bb.Max.y + GrabRadius));
		PopID();
		return Changed;
	}
}
