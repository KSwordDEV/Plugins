#pragma once

extern HANDLE DataCollectorThread;
extern HINSTANCE g_hInstance;
DWORD WINAPI DataCollectorEntry(LPVOID lpThreadParameter);

#ifdef __APPLE__
void MacPortEntryPoint(void *param);
#endif

#if defined(__linux__) || defined(__ANDROID__)
void LinuxPortEntryPoint(void *param);
#endif
