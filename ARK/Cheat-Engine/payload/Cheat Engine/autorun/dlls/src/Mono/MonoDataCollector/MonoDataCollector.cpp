#ifdef _WINDOWS
#include "stdafx.h"
#endif

#ifdef __linux__
#include "linuxport.h"
#endif

#ifdef __APPLE__
#include "macport.h"
#endif

#include "PipeServer.h"


HANDLE DataCollectorThread;
HINSTANCE g_hInstance;

typedef enum _THREADINFOCLASS {
    ThreadBasicInformation,
    ThreadTimes,
    ThreadPriority,
    ThreadBasePriority,
    ThreadAffinityMask,
    ThreadImpersonationToken,
    ThreadDescriptorTableEntry,
    ThreadEnableAlignmentFaultFixup,
    ThreadEventPair_Reusable,
    ThreadQuerySetWin32StartAddress,
    ThreadZeroTlsCell,
    ThreadPerformanceCount,
    ThreadAmILastThread,
    ThreadIdealProcessor,
    ThreadPriorityBoost,
    ThreadSetTlsArrayAddress,   // Obsolete
    ThreadIsIoPending,
    ThreadHideFromDebugger,
    ThreadBreakOnTermination,
    ThreadSwitchLegacyState,
    ThreadIsTerminated,
    ThreadLastSystemCall,
    ThreadIoPriority,
    ThreadCycleTime,
    ThreadPagePriority,
    ThreadActualBasePriority,
    ThreadTebInformation,
    ThreadCSwitchMon,          // Obsolete
    ThreadCSwitchPmu,
    ThreadWow64Context,
    ThreadGroupInformation,
    ThreadUmsInformation,      // UMS
    ThreadCounterProfiling,
    ThreadIdealProcessorEx,
    MaxThreadInfoClass
} THREADINFOCLASS;

#ifdef _WINDOWS
typedef int (NTAPI *ZWSETINFORMATIONTHREAD)(
    __in HANDLE ThreadHandle,
    __in THREADINFOCLASS ThreadInformationClass,
    __in_bcount(ThreadInformationLength) PVOID ThreadInformation,
    __in ULONG ThreadInformationLength
    );
#endif

#ifdef CUSTOM_DEBUG
FILE* CreateAndTestDebugConsole()
{
    FILE* f = nullptr;
#if DEBUG_CONSOLE
    AllocConsole();
    freopen_s(&f, "CONOUT$", "w", stdout);
#endif
    return f;
}
#endif



DWORD WINAPI DataCollectorEntry(LPVOID lpThreadParameter)
{
	CPipeServer *pw;
    
    
#ifdef _WINDOWS
#ifdef NDEBUG
	ZWSETINFORMATIONTHREAD ZwSetInformationThread=(ZWSETINFORMATIONTHREAD)GetProcAddress(GetModuleHandleA("ntdll.dll"), "ZwSetInformationThread");
	if (ZwSetInformationThread)
	{
		int r=ZwSetInformationThread(GetCurrentThread(), ThreadHideFromDebugger, NULL, 0);
		if (r!=0)
		{
			//OutputDebugStringA("No debug safety");
		}
	}
#endif
#endif


	OutputDebugString("DataCollectorEntry\n");

	OutputDebugString("creating new CPipeServer instance\n");
	pw=new CPipeServer();

#ifdef CUSTOM_DEBUG
    FILE* console = CreateAndTestDebugConsole();
    if (console)
        printf("Console created!\n");
#endif

    OutputDebugString("calling InitMono\n");
	InitMono();

	OutputDebugString("calling CreatePipeAndSpawnWorkers\n");
	pw->CreatePipeAndSpawnWorkers();

    OutputDebugString("Destroying PipeServer\n");
	DataCollectorThread=0;
	delete pw;	

#ifdef CUSTOM_DEBUG
    if (console)
        fclose(console);
#if DEBUG_CONSOLE
    FreeConsole();
#endif
#endif


	Sleep(1000);


#ifdef _WINDOWS
    OutputDebugString("Freeing Memory\n");
	FreeLibraryAndExitThread(g_hInstance, 0);


#endif
	return 0;
}

#ifdef __APPLE__
#include <syslog.h>
int logenabled=0;
void MacPortEntryPoint(void *param)
{
    
    pthread_setname_np("MonoDataCollector Thread");
    
    openlog((char*)"CEMDC", 0, LOG_USER);
    setlogmask(LOG_UPTO(LOG_DEBUG));
    logenabled=1;
    
    DataCollectorEntry(param);
    
}
#endif

#if defined(__linux__) || defined(__ANDROID__)
void LinuxPortEntryPoint(void *param)
{
    OutputDebugString("LinuxPortEntryPoint\n");

    DataCollectorEntry(param);
}
#endif

