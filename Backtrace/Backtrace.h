#pragma once
#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <dbghelp.h>
#include <vector>
#include <sstream>
#pragma comment(lib, "version.lib")  // for "VerQueryValue"

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
   int      maxDepth;
   DWORD    LoadModule(HANDLE hProcess, const char modulePath[], DWORD64 baseAddr, DWORD size);

   HANDLE   hProcess;
   DWORD    dwProcessId;
   HMODULE  hDbhHelp;

   typedef struct Entry
   {
      std::string function;
      DWORD       line;
      std::string file;
   };
   std::vector<Entry> callStack;
};