#pragma once
#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <dbghelp.h>
#include <vector>
#include <sstream>
#pragma comment(lib, "version.lib")  // for "VerQueryValue"

#define OPTIONS_ALL				0x3F
#define MAX_MODULE_NAME32		255
#define TH32CS_SNAPMODULE		0x00000008
#define BACKTRACE_MAX_NAMELEN   1024

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
#define GET_CURRENT_CONTEXT(c, contextFlags) \
  do { \
    memset(&c, 0, sizeof(CONTEXT)); \
    c.ContextFlags = contextFlags; \
    RtlCaptureContext(&c); \
} while(0);
#endif

class Backtrace
{
public:
   Backtrace();
   ~Backtrace();
   void ShowCallstack();
   std::string ToString();
private:
   void     Init(LPCSTR szSymPath);
   BOOL     GetModuleListPSAPI(HANDLE hProcess);
   DWORD    LoadModule(HANDLE hProcess, LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size);
   void     LoadModules();
   void     LoadModules(HANDLE hProcess, DWORD dwProcessId);

   HANDLE   m_hProcess;
   DWORD    m_dwProcessId;
   LPSTR    m_szSymPath;
   HMODULE  m_hDbhHelp;

   static BOOL __stdcall BacktraceReadProcMem(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);

   typedef struct CallstackEntry
   {
      DWORD64  offset;  // if 0, we have no valid entry
      CHAR     name[BACKTRACE_MAX_NAMELEN];
      CHAR     undName[BACKTRACE_MAX_NAMELEN];
      CHAR     undFullName[BACKTRACE_MAX_NAMELEN];
      DWORD64  offsetFromSmybol;
      DWORD    offsetFromLine;
      DWORD    lineNumber;
      CHAR     lineFileName[BACKTRACE_MAX_NAMELEN];
      DWORD    symType;
      LPCSTR   symTypeString;
   } CallstackEntry;

   std::vector<CallstackEntry> m_callStack;

#pragma pack( pop )
   // PSAPI
   typedef struct _MODULEINFO {
      LPVOID   lpBaseOfDll;
      DWORD    SizeOfImage;
      LPVOID   EntryPoint;
   } MODULEINFO, *LPMODULEINFO;
#pragma endregion
};