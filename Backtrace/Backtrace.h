#pragma once
#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <dbghelp.h>
#pragma comment(lib, "version.lib")  // for "VerQueryValue"

#define OPTIONS_ALL 0x3F
// **************************************** ToolHelp32 ************************
#define MAX_MODULE_NAME32   255
#define TH32CS_SNAPMODULE   0x00000008

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

#define STACKWALK_MAX_NAMELEN 1024

//from internal
typedef struct IMAGEHLP_MODULE64_V2 {
	DWORD    SizeOfStruct;           // set to sizeof(IMAGEHLP_MODULE64)
	DWORD64  BaseOfImage;            // base load address of module
	DWORD    ImageSize;              // virtual size of the loaded module
	DWORD    TimeDateStamp;          // date/time stamp from pe header
	DWORD    CheckSum;               // checksum from the pe header
	DWORD    NumSyms;                // number of symbols in the symbol table
	SYM_TYPE SymType;                // type of symbols loaded
	CHAR     ModuleName[32];         // module name
	CHAR     ImageName[256];         // image name
	CHAR     LoadedImageName[256];   // symbol file name
};
// SymCleanup()
typedef BOOL(__stdcall *tSC)(IN HANDLE hProcess);
// SymFunctionTableAccess64()
typedef PVOID(__stdcall *tSFTA)(HANDLE hProcess, DWORD64 AddrBase);
// SymGetLineFromAddr64()
typedef BOOL(__stdcall *tSGLFA)(IN HANDLE hProcess, IN DWORD64 dwAddr, OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINE64 Line);
// SymGetModuleBase64()
typedef DWORD64(__stdcall *tSGMB)(IN HANDLE hProcess, IN DWORD64 dwAddr);
// SymGetSymFromAddr64()
typedef BOOL(__stdcall *tSGSFA)(IN HANDLE hProcess, IN DWORD64 dwAddr, OUT PDWORD64 pdwDisplacement, OUT PIMAGEHLP_SYMBOL64 Symbol);
// SymInitialize()
typedef BOOL(__stdcall *tSI)(IN HANDLE hProcess, IN PSTR UserSearchPath, IN BOOL fInvadeProcess);
// SymLoadModule64()
typedef DWORD64(__stdcall *tSLM)(IN HANDLE hProcess, IN HANDLE hFile, IN PSTR ImageName, IN PSTR ModuleName, IN DWORD64 BaseOfDll, IN DWORD SizeOfDll);
// StackWalk64()
typedef BOOL(__stdcall *tSW)(DWORD MachineType, HANDLE hProcess, HANDLE hThread, LPSTACKFRAME64 StackFrame, PVOID ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine, PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine, PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);
//tSC pSC;
//tSFTA pSFTA;
//tSGLFA pSGLFA;
//tSGMB pSGMB;
//tSGSFA pSGSFA;
//tSI pSI;
//tSLM pSLM;
//tSW pSW;

//TODO: delete Internal class
class BacktraceInternal;  // forward
class Backtrace
{
public:
   Backtrace();
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
   BOOL ShowCallstack();

private:
   void Init(LPCSTR szSymPath);
   BOOL GetModuleListTH32(HANDLE hProcess, DWORD pid);
   BOOL GetModuleListPSAPI(HANDLE hProcess);
   DWORD LoadModule(HANDLE hProcess, LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size);
   void LoadModules(HANDLE hProcess, DWORD dwProcessId);
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

   HANDLE m_hProcess;
   DWORD m_dwProcessId;
   LPSTR m_szSymPath;

   static BOOL __stdcall myReadProcMem(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);

   friend BacktraceInternal;

#pragma region Backtrace Internal

   HMODULE m_hDbhHelp;

   tSC pSC;
   tSFTA pSFTA;
   tSGLFA pSGLFA;
   tSGMB pSGMB;
   tSGSFA pSGSFA;
   tSI pSI;
   tSLM pSLM;
   tSW pSW;

   // **************************************** ToolHelp32 ************************
#pragma pack( push, 8 )
   typedef struct tagMODULEENTRY32
   {
      DWORD   dwSize;
      DWORD   th32ModuleID;       // This module
      DWORD   th32ProcessID;      // owning process
      DWORD   GlblcntUsage;       // Global usage count on the module
      DWORD   ProccntUsage;       // Module usage count in th32ProcessID's context
      BYTE  * modBaseAddr;        // Base address of module in th32ProcessID's context
      DWORD   modBaseSize;        // Size in bytes of module starting at modBaseAddr
      HMODULE hModule;            // The hModule of this module in th32ProcessID's context
      char    szModule[MAX_MODULE_NAME32 + 1];
      char    szExePath[MAX_PATH];
   } MODULEENTRY32;
   typedef MODULEENTRY32 *  PMODULEENTRY32;
   typedef MODULEENTRY32 *  LPMODULEENTRY32;
#pragma pack( pop )

   // **************************************** PSAPI ************************
   typedef struct _MODULEINFO {
      LPVOID lpBaseOfDll;
      DWORD SizeOfImage;
      LPVOID EntryPoint;
   } MODULEINFO, *LPMODULEINFO;
#pragma endregion

};