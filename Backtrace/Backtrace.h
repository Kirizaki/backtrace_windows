#pragma once
#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <dbghelp.h>
#include <vector>
#include <sstream>
#pragma comment(lib, "version.lib")  // for "VerQueryValue"

#define BACKTRACE_MAX_NAMELEN 1024

typedef struct CallstackEntry
{
   DWORD64  offset;
   CHAR     function[BACKTRACE_MAX_NAMELEN];
   DWORD64  offsetFromSmybol;
   DWORD    offsetFromLine;
   DWORD    line;
   CHAR     file[BACKTRACE_MAX_NAMELEN];
};

class Backtrace
{
public:
   Backtrace(int maxDepth = 32);
   ~Backtrace();
   std::string GetBacktrace();

private:
   void     Callstack();
   void     LoadModule();
   void     LoadDBGHELP();
   void     LoadModuleInformation(HANDLE hProcess);
   int      m_maxDepth;
   DWORD    LoadModule(HANDLE hProcess, const std::string& img, DWORD64 baseAddr, DWORD size);

   HANDLE   m_hProcess;
   DWORD    m_dwProcessId;
   HMODULE  m_hDbhHelp;

   std::vector<CallstackEntry> m_callStack;
};