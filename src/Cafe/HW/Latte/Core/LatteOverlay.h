#pragma once

void LatteOverlay_init();
void LatteOverlay_render(bool pad_view);
void LatteOverlay_updateStats(double fps, sint32 drawcalls);
void LatteOverlay_updateStatsPerFrame();

void LatteOverlay_pushNotification(const std::string& text, sint32 duration);