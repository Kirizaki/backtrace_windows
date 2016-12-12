#pragma once

#include <windows.h>
#include <iostream>

// special defines for VC5/6 (if no actual PSDK is installed):
#if _MSC_VER < 1300
typedef unsigned __int64 DWORD64, *PDWORD64;
#if defined(_WIN64)
typedef unsigned __int64 SIZE_T, *PSIZE_T;
#else
typedef unsigned long SIZE_T, *PSIZE_T;
#endif
#endif  // _MSC_VER < 1300

#ifdef _M_IX86
#define GET_CURRENT_CONTEXT(c, contextFlags) \
  do { \
    memset(&c, 0, sizeof(CONTEXT)); \
    c.ContextFlags = contextFlags; \
    __asm    call x \
    __asm x: pop eax \
    __asm    mov c.Eip, eax \
    __asm    mov c.Ebp, ebp \
    __asm    mov c.Esp, esp \
  } while(0);
#else

// The following is defined for x86 (XP and higher), x64 and IA64:
#define GET_CURRENT_CONTEXT(c, contextFlags) \
  do { \
    memset(&c, 0, sizeof(CONTEXT)); \
    c.ContextFlags = contextFlags; \
    RtlCaptureContext(&c); \
} while(0);
#endif

//TODO: delete Internal class
class BacktraceInternal;  // forward
class Backtrace
{
public:

   const static int OptionsAll = 0x3F;

   Backtrace(
      int options = OptionsAll, // 'int' is by design, to combine the enum-flags
      LPCSTR szSymPath = NULL,
      DWORD dwProcessId = GetCurrentProcessId(),
      HANDLE hProcess = GetCurrentProcess()
   );
   Backtrace(DWORD dwProcessId, HANDLE hProcess);
   virtual ~Backtrace();

   typedef BOOL(__stdcall *PReadProcessMemoryRoutine)(
      HANDLE      hProcess,
      DWORD64     qwBaseAddress,
      PVOID       lpBuffer,
      DWORD       nSize,
      LPDWORD     lpNumberOfBytesRead,
      LPVOID      pUserData  // optional data, which was passed in "ShowCallstack"
      );

   BOOL LoadModules();

   BOOL ShowCallstack(
      HANDLE hThread = GetCurrentThread(),
      PReadProcessMemoryRoutine readMemoryFunction = NULL,
      LPVOID pUserData = NULL  // optional to identify some data in the 'readMemoryFunction'-callback
   );

#if _MSC_VER >= 1300
   // due to some reasons, the "STACKWALK_MAX_NAMELEN" must be declared as "public"
   // in older compilers in order to use it... starting with VC7 we can declare it as "protected"
protected:
#endif
   enum { STACKWALK_MAX_NAMELEN = 1024 }; // max name length for found symbols

protected:
   // Entry for each Callstack-Entry
   typedef struct CallstackEntry
   {
      DWORD64 offset;  // if 0, we have no valid entry
      CHAR name[STACKWALK_MAX_NAMELEN];
      CHAR undName[STACKWALK_MAX_NAMELEN];
      CHAR undFullName[STACKWALK_MAX_NAMELEN];
      DWORD64 offsetFromSmybol;
      DWORD offsetFromLine;
      DWORD lineNumber;
      CHAR lineFileName[STACKWALK_MAX_NAMELEN];
      DWORD symType;
      LPCSTR symTypeString;
      CHAR moduleName[STACKWALK_MAX_NAMELEN];
      DWORD64 baseOfImage;
      CHAR loadedImageName[STACKWALK_MAX_NAMELEN];
   } CallstackEntry;

   typedef enum CallstackEntryType
   {
	   firstEntry,
	   nextEntry,
	   lastEntry,
   };

   BacktraceInternal *m_bt;
   HANDLE m_hProcess;
   DWORD m_dwProcessId;
   /*BOOL m_modulesLoaded;*/
   LPSTR m_szSymPath;

   static BOOL __stdcall myReadProcMem(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);

   friend BacktraceInternal;
};