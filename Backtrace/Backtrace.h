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
   Backtrace(const int& maxDepth = 32);
   ~Backtrace();
   std::string GetBacktrace();

private:
   void     Callstack();
   int      maxDepth;

   HANDLE   hProcess;
   DWORD    dwProcessId;
   HMODULE  hDbgHelp;

   typedef struct Entry
   {
      std::string function;
      DWORD       line;
      std::string file;
      int         count;
   };
   std::vector<Entry> callStack;
};