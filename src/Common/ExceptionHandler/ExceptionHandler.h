#pragma once

void ExceptionHandler_Init();

bool CrashLog_Create();
void CrashLog_SetOutputChannels(bool writeToStdErr, bool writeToLogTxt);
void CrashLog_WriteLine(std::string_view text, bool newLine = true);
void CrashLog_WriteHeader(const char* header);

void ExceptionHandler_LogGeneralInfo();
