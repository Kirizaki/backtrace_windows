#include "Backtrace.h"

// Normally it should be enough to use 'CONTEXT_FULL' (better would be 'CONTEXT_ALL')
#define USED_CONTEXT_FLAGS CONTEXT_FULL

// TODO: Replace with initialization list
Backtrace::Backtrace()
{
   m_dwProcessId = GetCurrentProcessId();
   m_szSymPath = NULL;
   m_hDbhHelp = NULL;
   pSC = NULL;
   m_hProcess = GetCurrentProcess();
   m_szSymPath = NULL;
   pSFTA = NULL;
   pSGLFA = NULL;
   pSGMB = NULL;
   pSGSFA = NULL;
   pSI = NULL;
   pSLM = NULL;
   pSW = NULL;
}

Backtrace::~Backtrace()
{
   if (m_szSymPath != NULL)
      free(m_szSymPath);
   m_szSymPath = NULL;
   if (pSC != NULL)
      pSC(m_hProcess);
   if (m_hDbhHelp != NULL)
      FreeLibrary(m_hDbhHelp);
   m_hDbhHelp = NULL;
   if (m_szSymPath != NULL)
      free(m_szSymPath);
   m_szSymPath = NULL;
}

BOOL Backtrace::LoadModules()
{
   // Build the sym-path:
   char *szSymPath = NULL;

   const size_t nSymPathLen = 4096;
   szSymPath = (char*)malloc(nSymPathLen);
   szSymPath[0] = 0;

   strcat_s(szSymPath, nSymPathLen, ".;");

   const size_t nTempLen = 1024;
   char szTemp[nTempLen];
   // Now add the current directory:
   if (GetCurrentDirectoryA(nTempLen, szTemp) > 0)
   {
      szTemp[nTempLen - 1] = 0;
      strcat_s(szSymPath, nSymPathLen, szTemp);
      strcat_s(szSymPath, nSymPathLen, ";");
   }

   // Now add the path for the main-module:
   if (GetModuleFileNameA(NULL, szTemp, nTempLen) > 0)
   {
      szTemp[nTempLen - 1] = 0;
      for (char *p = (szTemp + strlen(szTemp) - 1); p >= szTemp; --p)
      {
         // locate the rightmost path separator
         if ((*p == '\\') || (*p == '/') || (*p == ':'))
         {
            *p = 0;
            break;
         }
      }  // for (search for path separator...)
      if (strlen(szTemp) > 0)
      {
         strcat_s(szSymPath, nSymPathLen, szTemp);
         strcat_s(szSymPath, nSymPathLen, ";");
      }
   }

   try
   {
      Init(szSymPath);
      if (szSymPath != NULL)
         free(szSymPath);
      szSymPath = NULL;
   }
   catch(std::exception &e)
   {
      //_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
      std::cout << "Error while initializing dbghelp.dll: " << e.what() << std::endl;
      return FALSE;
   }

   try
   {
      // TODO: Replace with Backtrace::LoadWINAPI [?]
      LoadModules(this->m_hProcess, this->m_dwProcessId);
   }
   catch (std::exception &e)
   {
      std::cout << "Modules seems to be already loaded: " << e.what() << std::endl;
      return FALSE;
   }
   return TRUE;
}

BOOL Backtrace::ShowCallstack()
{
   CONTEXT c;
   CallstackEntry csEntry;
   IMAGEHLP_SYMBOL64 *pSym = NULL;
   IMAGEHLP_MODULE64_V2 Module;
   IMAGEHLP_LINE64 Line;
   int frameNum;

   LoadModules();  // ignore the result...

   if (m_hDbhHelp == NULL)
   {
      SetLastError(ERROR_DLL_INIT_FAILED);
      return FALSE;
   }

   GET_CURRENT_CONTEXT(c, USED_CONTEXT_FLAGS);

   // init STACKFRAME for first call
   STACKFRAME64 s; // in/out stackframe
   memset(&s, 0, sizeof(s));
   DWORD imageType;
#ifdef _M_IX86
   imageType = IMAGE_FILE_MACHINE_I386;
   s.AddrPC.Offset = c.Eip;
   s.AddrPC.Mode = AddrModeFlat;
   s.AddrFrame.Offset = c.Ebp;
   s.AddrFrame.Mode = AddrModeFlat;
   s.AddrStack.Offset = c.Esp;
   s.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
   imageType = IMAGE_FILE_MACHINE_AMD64;
   s.AddrPC.Offset = c.Rip;
   s.AddrPC.Mode = AddrModeFlat;
   s.AddrFrame.Offset = c.Rsp;
   s.AddrFrame.Mode = AddrModeFlat;
   s.AddrStack.Offset = c.Rsp;
   s.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
   imageType = IMAGE_FILE_MACHINE_IA64;
   s.AddrPC.Offset = c.StIIP;
   s.AddrPC.Mode = AddrModeFlat;
   s.AddrFrame.Offset = c.IntSp;
   s.AddrFrame.Mode = AddrModeFlat;
   s.AddrBStore.Offset = c.RsBSP;
   s.AddrBStore.Mode = AddrModeFlat;
   s.AddrStack.Offset = c.IntSp;
   s.AddrStack.Mode = AddrModeFlat;
#else
#error "Platform not supported!"
#endif

   pSym = (IMAGEHLP_SYMBOL64 *)malloc(sizeof(IMAGEHLP_SYMBOL64) + BACKTRACE_MAX_NAMELEN);
   if (!pSym)
   {
      free(pSym); // not enough memory...
      return TRUE;
   }
   memset(pSym, 0, sizeof(IMAGEHLP_SYMBOL64) + BACKTRACE_MAX_NAMELEN);
   pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
   pSym->MaxNameLength = BACKTRACE_MAX_NAMELEN;

   memset(&Line, 0, sizeof(Line));
   Line.SizeOfStruct = sizeof(Line);

   memset(&Module, 0, sizeof(Module));
   Module.SizeOfStruct = sizeof(Module);

   for (frameNum = 0; ; ++frameNum)
   {
      // get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
      // if this returns ERROR_INVALID_ADDRESS (487) or ERROR_NOACCESS (998), you can
      // assume that either you are done, or that the stack is so hosed that the next
      // deeper frame could not be found.
      // CONTEXT need not to be suplied if imageTyp is IMAGE_FILE_MACHINE_I386!
      if (!pSW(imageType, this->m_hProcess, GetCurrentThread(), &s, &c, myReadProcMem, pSFTA, pSGMB, NULL))
      {
         //this->OnDbgHelpErr("StackWalk64", GetLastError(), s.AddrPC.Offset);
         //_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
         break;
      }

      csEntry.offset = s.AddrPC.Offset;
      csEntry.name[0] = 0;
      csEntry.undName[0] = 0;
      csEntry.undFullName[0] = 0;
      csEntry.offsetFromSmybol = 0;
      csEntry.offsetFromLine = 0;
      csEntry.lineFileName[0] = 0;
      csEntry.lineNumber = 0;
      csEntry.loadedImageName[0] = 0;
      csEntry.moduleName[0] = 0;
      if (s.AddrPC.Offset == s.AddrReturn.Offset)
      {
         //_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
         //this->OnDbgHelpErr("StackWalk64-Endless-Callstack!", 0, s.AddrPC.Offset);
         break;
      }
      if (s.AddrPC.Offset != 0)
      {
         // we seem to have a valid PC
         // show procedure info (SymGetSymFromAddr64())
         if (pSGSFA(this->m_hProcess, s.AddrPC.Offset, &(csEntry.offsetFromSmybol), pSym) != FALSE)
         {
            // TODO: Mache dies sicher...!
            strcpy_s(csEntry.name, pSym->Name);
         }
         else
         {
            //_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
            //this->OnDbgHelpErr("SymGetSymFromAddr64", GetLastError(), s.AddrPC.Offset);
         }

         // show line number info, NT5.0-method (SymGetLineFromAddr64())
         if (pSGLFA != NULL)
         { // yes, we have SymGetLineFromAddr64()
            if (pSGLFA(this->m_hProcess, s.AddrPC.Offset, &(csEntry.offsetFromLine), &Line) != FALSE)
            {
               csEntry.lineNumber = Line.LineNumber;
               // TODO: Mache dies sicher...!
               strcpy_s(csEntry.lineFileName, Line.FileName);
            }
            else
            {
               ;//_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
               //this->OnDbgHelpErr("SymGetLineFromAddr64", GetLastError(), s.AddrPC.Offset);
            }
         } // yes, we have SymGetLineFromAddr64()
#if API_VERSION_NUMBER >= 9
         csEntry.symTypeString = "DIA";
#else
         csEntry.symTypeString = NULL;
#endif
            // TODO: Mache dies sicher...!
            strcpy_s(csEntry.moduleName, Module.ModuleName);
            csEntry.baseOfImage = Module.BaseOfImage;
            strcpy_s(csEntry.loadedImageName, Module.LoadedImageName);
      } // we seem to have a valid PC

      CallstackEntryType et = nextEntry;
      if (frameNum == 0)
         et = firstEntry;

      if ((et != lastEntry) && (csEntry.offset != 0))
      {
         if (csEntry.name[0] == 0)
            strcpy_s(csEntry.name, "(function-name not available)");
         if (csEntry.undName[0] != 0)
            strcpy_s(csEntry.name, csEntry.undName);
         if (csEntry.undFullName[0] != 0)
            strcpy_s(csEntry.name, csEntry.undFullName);
         if (csEntry.lineFileName[0] == 0)
         {
            strcpy_s(csEntry.lineFileName, "(filename not available)");
            if (csEntry.moduleName[0] == 0)
               strcpy_s(csEntry.moduleName, "(module-name not available)");
         }
         std::cout << (LPVOID)csEntry.offset << " " << "[bt]: " << csEntry.lineFileName << ": " << csEntry.name << std::endl;
      }
   } // for ( frameNum )

   if (NULL != pSym)
      free(pSym);

   return TRUE;
}

BOOL __stdcall Backtrace::myReadProcMem(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead)
{
   SIZE_T st;
   BOOL bRet = ReadProcessMemory(hProcess, (LPVOID)qwBaseAddress, lpBuffer, nSize, &st);
   *lpNumberOfBytesRead = (DWORD)st;
   //printf("ReadMemory: hProcess: %p, baseAddr: %p, buffer: %p, size: %d, read: %d, result: %d\n", hProcess, (LPVOID) qwBaseAddress, lpBuffer, nSize, (DWORD) st, (DWORD) bRet);
   return bRet;
}

void Backtrace::Init(LPCSTR szSymPath)
{
   // load dll
   if (m_hDbhHelp == NULL)
      m_hDbhHelp = LoadLibrary(_T("dbghelp.dll"));

   // TODO: replace by a map of functors[?]
   pSI = (tSI)GetProcAddress(m_hDbhHelp, "SymInitialize");
   pSC = (tSC)GetProcAddress(m_hDbhHelp, "SymCleanup");
   pSW = (tSW)GetProcAddress(m_hDbhHelp, "StackWalk64");
   pSFTA = (tSFTA)GetProcAddress(m_hDbhHelp, "SymFunctionTableAccess64");
   pSGLFA = (tSGLFA)GetProcAddress(m_hDbhHelp, "SymGetLineFromAddr64");
   pSGMB = (tSGMB)GetProcAddress(m_hDbhHelp, "SymGetModuleBase64");         //
   pSGSFA = (tSGSFA)GetProcAddress(m_hDbhHelp, "SymGetSymFromAddr64");      // Function NAME
   pSLM = (tSLM)GetProcAddress(m_hDbhHelp, "SymLoadModule64");         // Process DATA

   // Initialize DLL api
   m_szSymPath = _strdup(szSymPath);
   if (this->pSI(m_hProcess, m_szSymPath, FALSE) == FALSE)
   {
      FreeLibrary(m_hDbhHelp);
      m_hDbhHelp = NULL;
      pSC = NULL;
      throw std::exception("Could not initialize symbols..");
   }
}

BOOL Backtrace::GetModuleListTH32(HANDLE hProcess, DWORD pid)
{
   // CreateToolhelp32Snapshot()
   typedef HANDLE(__stdcall *tCT32S)(DWORD dwFlags, DWORD th32ProcessID);
   // Module32First()
   typedef BOOL(__stdcall *tM32F)(HANDLE hSnapshot, MODULEENTRY32* moduleEntry);
   // Module32Next()
   typedef BOOL(__stdcall *tM32N)(HANDLE hSnapshot, MODULEENTRY32* moduleEntry);

   // try both dlls...
   const TCHAR *dllname[] = { _T("kernel32.dll"), _T("tlhelp32.dll") };
   HINSTANCE hToolhelp = NULL;
   tCT32S pCT32S = NULL;
   tM32F pM32F = NULL;
   tM32N pM32N = NULL;

   HANDLE hSnap;
   MODULEENTRY32 me;
   me.dwSize = sizeof(me);
   BOOL keepGoing;
   size_t i;

   for (i = 0; i < (sizeof(dllname) / sizeof(dllname[0])); i++)
   {
      hToolhelp = LoadLibrary(dllname[i]);
      if (hToolhelp == NULL)
         continue;
      pCT32S = (tCT32S)GetProcAddress(hToolhelp, "CreateToolhelp32Snapshot");
      pM32F = (tM32F)GetProcAddress(hToolhelp, "Module32First");
      pM32N = (tM32N)GetProcAddress(hToolhelp, "Module32Next");
      if ((pCT32S != NULL) && (pM32F != NULL) && (pM32N != NULL))
         break; // found the functions!
      FreeLibrary(hToolhelp);
      hToolhelp = NULL;
   }

   if (hToolhelp == NULL)
      return FALSE;

   hSnap = pCT32S(TH32CS_SNAPMODULE, pid);
   if (hSnap == (HANDLE)-1)
      return FALSE;

   keepGoing = !!pM32F(hSnap, &me);
   int cnt = 0;
   while (keepGoing)
   {
      this->LoadModule(hProcess, me.szExePath, me.szModule, (DWORD64)me.modBaseAddr, me.modBaseSize);
      cnt++;
      keepGoing = !!pM32N(hSnap, &me);
   }
   CloseHandle(hSnap);
   FreeLibrary(hToolhelp);
   return (cnt <= 0) ? FALSE : TRUE;
}  // GetModuleListTH32

BOOL Backtrace::GetModuleListPSAPI(HANDLE hProcess)
{
   // EnumProcessModules()
   typedef BOOL(__stdcall *tEPM)(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);
   // GetModuleFileNameEx()
   typedef DWORD(__stdcall *tGMFNE)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize);
   // GetModuleBaseName()
   typedef DWORD(__stdcall *tGMBN)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize);
   // GetModuleInformation()
   typedef BOOL(__stdcall *tGMI)(HANDLE hProcess, HMODULE hModule, LPMODULEINFO pmi, DWORD nSize);

   HINSTANCE hPsapi;
   tEPM pEPM;
   tGMFNE pGMFNE;
   tGMBN pGMBN;
   tGMI pGMI;

   DWORD i;
   //ModuleEntry e;
   DWORD cbNeeded;
   MODULEINFO mi;
   HMODULE *hMods = 0;
   char *tt = NULL;
   char *tt2 = NULL;
   const SIZE_T TTBUFLEN = 8096;
   int cnt = 0;

   hPsapi = LoadLibrary(_T("psapi.dll"));
   if (hPsapi == NULL)
      return FALSE;

   pEPM = (tEPM)GetProcAddress(hPsapi, "EnumProcessModules");
   pGMFNE = (tGMFNE)GetProcAddress(hPsapi, "GetModuleFileNameExA");
   pGMBN = (tGMFNE)GetProcAddress(hPsapi, "GetModuleBaseNameA");
   pGMI = (tGMI)GetProcAddress(hPsapi, "GetModuleInformation");
   if ((pEPM == NULL) || (pGMFNE == NULL) || (pGMBN == NULL) || (pGMI == NULL))
   {
      // we couldn�t find all functions
      FreeLibrary(hPsapi);
      return FALSE;
   }

   hMods = (HMODULE*)malloc(sizeof(HMODULE) * (TTBUFLEN / sizeof HMODULE));
   tt = (char*)malloc(sizeof(char) * TTBUFLEN);
   tt2 = (char*)malloc(sizeof(char) * TTBUFLEN);
   if ((hMods == NULL) || (tt == NULL) || (tt2 == NULL))
   {
      if (hPsapi != NULL) FreeLibrary(hPsapi);
      if (tt2 != NULL) free(tt2);
      if (tt != NULL) free(tt);
      if (hMods != NULL) free(hMods);

      return cnt != 0;
   }

   if (!pEPM(hProcess, hMods, TTBUFLEN, &cbNeeded))
   {
      //_ftprintf(fLogFile, _T("%lu: EPM failed, GetLastError = %lu\n"), g_dwShowCount, gle );
      if (hPsapi != NULL) FreeLibrary(hPsapi);
      if (tt2 != NULL) free(tt2);
      if (tt != NULL) free(tt);
      if (hMods != NULL) free(hMods);

      return cnt != 0;
   }

   if (cbNeeded > TTBUFLEN)
   {
      //_ftprintf(fLogFile, _T("%lu: More than %lu module handles. Huh?\n"), g_dwShowCount, lenof( hMods ) );
      if (hPsapi != NULL) FreeLibrary(hPsapi);
      if (tt2 != NULL) free(tt2);
      if (tt != NULL) free(tt);
      if (hMods != NULL) free(hMods);

      return cnt != 0;
   }

   for (i = 0; i < cbNeeded / sizeof hMods[0]; i++)
   {
      // base address, size
      pGMI(hProcess, hMods[i], &mi, sizeof mi);
      // image file name
      tt[0] = 0;
      pGMFNE(hProcess, hMods[i], tt, TTBUFLEN);
      // module name
      tt2[0] = 0;
      pGMBN(hProcess, hMods[i], tt2, TTBUFLEN);

      DWORD dwRes = this->LoadModule(hProcess, tt, tt2, (DWORD64)mi.lpBaseOfDll, mi.SizeOfImage);
      if (dwRes != ERROR_SUCCESS)
         ;//_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
          //this->m_parent->OnDbgHelpErr("LoadModule", dwRes, 0);
      cnt++;
   }

   if (hPsapi != NULL) FreeLibrary(hPsapi);
   if (tt2 != NULL) free(tt2);
   if (tt != NULL) free(tt);
   if (hMods != NULL) free(hMods);

   return cnt != 0;
}  // GetModuleListPSAPI

DWORD Backtrace::LoadModule(HANDLE hProcess, LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size)
{
   CHAR *szImg = _strdup(img);
   CHAR *szMod = _strdup(mod);
   DWORD result = ERROR_SUCCESS;
   if ((szImg == NULL) || (szMod == NULL))
      result = ERROR_NOT_ENOUGH_MEMORY;
   else
   {
      if (pSLM(hProcess, 0, szImg, szMod, baseAddr, size) == 0)
         result = GetLastError();
   }
   ULONGLONG fileVersion = 0;
   if (szImg != NULL)
   {
      VS_FIXEDFILEINFO *fInfo = NULL;
      DWORD dwHandle;
      DWORD dwSize = GetFileVersionInfoSizeA(szImg, &dwHandle);
      if (dwSize > 0)
      {
         LPVOID vData = malloc(dwSize);
         if (vData != NULL)
         {
            if (GetFileVersionInfoA(szImg, dwHandle, dwSize, vData) != 0)
            {
               UINT len;
               TCHAR szSubBlock[] = _T("\\");
               if (VerQueryValue(vData, szSubBlock, (LPVOID*)&fInfo, &len) == 0)
                  fInfo = NULL;
               else
               {
                  fileVersion = ((ULONGLONG)fInfo->dwFileVersionLS) + ((ULONGLONG)fInfo->dwFileVersionMS << 32);
               }
            }
            free(vData);
         }
      }
   }
   if (szImg != NULL) free(szImg);
   if (szMod != NULL) free(szMod);
   return result;
}

void Backtrace::LoadModules(HANDLE hProcess, DWORD dwProcessId)
{
   // first try toolhelp32
   if (GetModuleListTH32(hProcess, dwProcessId))
      return;
   // then try psapi
   if (GetModuleListPSAPI(hProcess))
      return;
   throw std::exception("Could not initialize ToolHelp32 and PsApi..");
}