#pragma once

void VsyncDriver_startThread(void(*cbVSync)());
void VsyncDriver_notifyWindowPosChanged();