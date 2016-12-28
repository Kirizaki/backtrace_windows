#pragma once
#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <dbghelp.h>
#include <vector>
#include <sstream>
#pragma comment(lib, "version.lib")  // for "VerQueryValue"

#define BACKTRACE_MAX_NAMELEN    1024

class Backtrace
{
public:
   Backtrace(int maxDepth = 32);
   ~Backtrace();
   void ShowCallstack();
   std::string ToString();
   typedef struct CallstackEntry
   {
      DWORD64  offset;  // if 0, we have no valid entry
      CHAR     name[BACKTRACE_MAX_NAMELEN];
      DWORD64  offsetFromSmybol;
      DWORD    offsetFromLine;
      DWORD    lineNumber;
      CHAR     lineFileName[BACKTRACE_MAX_NAMELEN];
   } CallstackEntry;
private:
   int      m_maxDepth;
   void     Init(LPCSTR szSymPath);
   DWORD    LoadModule(HANDLE hProcess, LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size);
   void     LoadModules();
   void     LoadModulesPSAPI(HANDLE hProcess);

   HANDLE   m_hProcess;
   DWORD    m_dwProcessId;
   LPSTR    m_szSymPath;
   HMODULE  m_hDbhHelp;

   static BOOL __stdcall BacktraceReadProcMem(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);

   std::vector<CallstackEntry> m_callStack;
};