#pragma once

void LatteTiming_setCustomVsyncFrequency(sint32 frequency);
void LatteTiming_disableCustomVsyncFrequency();
bool LatteTiming_getCustomVsyncFrequency(sint32& customFrequency);

void LatteTiming_EnableHostDrivenVSync();
void LatteTiming_DisableHostDrivenVSync();
void LatteTiming_ResetHostVsyncDetection();
void LatteTiming_NotifyHostVSync();
