#pragma once
#include "imgui.h"

void ImRotateStart();
ImVec2 ImRotationCenter();
void ImRotateEnd(float rad, ImVec2 center = ImRotationCenter());

inline ImVec2 operator-(const ImVec2& l, const ImVec2& r) { return{ l.x - r.x, l.y - r.y }; }
inline bool operator<(const ImVec2& lhs, const ImVec2& rhs) { return lhs.x < rhs.x; }
inline bool operator>=(const ImVec2& lhs, const ImVec2& rhs) { return lhs.x >= rhs.x; }

void ImGui_PrecacheFonts();
ImFont* ImGui_GetFont(float size);
void ImGui_UpdateWindowInformation(bool mainWindow);