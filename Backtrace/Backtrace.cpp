#include "Backtrace.h"
#include <memory>

#define USED_CONTEXT_FLAGS CONTEXT_ALL

namespace
{
   // TODO: Try to replace with template<> struct
#ifdef _M_IX86
#define GET_CURRENT_CONTEXT(context, contextFlags) \
  do { \
    memset(&context, 0, sizeof(CONTEXT)); \
    context.ContextFlags = contextFlags; \
    __asm    call x \
    __asm x: pop eax \
    __asm    mov context.Eip, eax \
    __asm    mov context.Ebp, ebp \
    __asm    mov context.Esp, esp \
  } while(0);
#else
#define GET_CURRENT_CONTEXT(context, contextFlags) \
  do { \
    memset(&context, 0, sizeof(CONTEXT)); \
    context.ContextFlags = contextFlags; \
    RtlCaptureContext(&context); \
} while(0);
#endif

   typedef BOOL(__stdcall *tSC)(IN HANDLE hProcess); // SymCleanup()
   typedef PVOID(__stdcall *tSFTA)(HANDLE hProcess, DWORD64 AddrBase);// SymFunctionTableAccess64()
   typedef BOOL(__stdcall *tSGLFA)(IN HANDLE hProcess, IN DWORD64 dwAddr, OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINE64 Line);// SymGetLineFromAddr64()
   typedef DWORD64(__stdcall *tSGMB)(IN HANDLE hProcess, IN DWORD64 dwAddr);// SymGetModuleBase64()
   typedef BOOL(__stdcall *tSGSFA)(IN HANDLE hProcess, IN DWORD64 dwAddr, OUT PDWORD64 pdwDisplacement, OUT PIMAGEHLP_SYMBOL64 Symbol);// SymGetSymFromAddr64()
   typedef BOOL(__stdcall *tSI)(IN HANDLE hProcess, IN PSTR UserSearchPath, IN BOOL fInvadeProcess);// SymInitialize()
   typedef DWORD64(__stdcall *tSLM)(IN HANDLE hProcess, IN HANDLE hFile, IN PSTR ImageName, IN PSTR ModuleName, IN DWORD64 BaseOfDll, IN DWORD SizeOfDll);// SymLoadModule64()
   typedef BOOL(__stdcall *tSW)(DWORD MachineType, HANDLE hProcess, HANDLE hThread, LPSTACKFRAME64 StackFrame, PVOID ContextRecord,
      PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine, PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
      PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine, PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);// StackWalk64()

   tSC      pSC;
   tSFTA    pSFTA;
   tSGLFA   pSGLFA;
   tSGMB    pSGMB;
   tSGSFA   pSGSFA;
   tSI      pSI;
   tSLM     pSLM;
   tSW      pSW;

   void InitStackFrame(const CONTEXT& context, DWORD& imageType, STACKFRAME64& stackFrame)
   {
#ifdef _M_IX86
      imageType = IMAGE_FILE_MACHINE_I386;
      stackFrame.AddrPC.Offset = context.Eip;
      stackFrame.AddrPC.Mode = AddrModeFlat;
      stackFrame.AddrFrame.Offset = context.Ebp;
      stackFrame.AddrFrame.Mode = AddrModeFlat;
      stackFrame.AddrStack.Offset = context.Esp;
      stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
      imageType = IMAGE_FILE_MACHINE_AMD64;
      stackFrame.AddrPC.Offset = context.Rip;
      stackFrame.AddrPC.Mode = AddrModeFlat;
      stackFrame.AddrFrame.Offset = context.Rsp;
      stackFrame.AddrFrame.Mode = AddrModeFlat;
      stackFrame.AddrStack.Offset = context.Rsp;
      stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
      imageType = IMAGE_FILE_MACHINE_IA64;
      stackFrame.AddrPC.Offset = context.StIIP;
      stackFrame.AddrPC.Mode = AddrModeFlat;
      stackFrame.AddrFrame.Offset = context.IntSp;
      stackFrame.AddrFrame.Mode = AddrModeFlat;
      stackFrame.AddrBStore.Offset = context.RsBSP;
      stackFrame.AddrBStore.Mode = AddrModeFlat;
      stackFrame.AddrStack.Offset = context.IntSp;
      stackFrame.AddrStack.Mode = AddrModeFlat;
#else
#error "Platform not supported!"
#endif
   }

   void GetLineFromAddr64(const HANDLE& hProcess, const STACKFRAME64& stackFrame, CallstackEntry& csEntry)
   {
      if (pSGLFA == NULL)
         return;

      IMAGEHLP_LINE64 Line;
      memset(&Line, 0, sizeof(Line));
      Line.SizeOfStruct = sizeof(Line);
      if (pSGLFA(hProcess, stackFrame.AddrPC.Offset, &(csEntry.offsetFromLine), &Line) != FALSE)
      {
         csEntry.line = Line.LineNumber;
         // TODO: Mache dies sicher...!
         strcpy_s(csEntry.file, Line.FileName);
      }
      else
      {
         ;//_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
          //this->OnDbgHelpErr("SymGetLineFromAddr64", GetLastError(), s.AddrPC.Offset);
      }
   }

   BOOL __stdcall ReadProcMem(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead)
   {
      SIZE_T st;
      BOOL bRet = ReadProcessMemory(hProcess, (LPVOID)qwBaseAddress, lpBuffer, nSize, &st);
      *lpNumberOfBytesRead = (DWORD)st;
      return bRet;
   }
}

Backtrace::Backtrace(int maxDepth) :
   m_maxDepth(maxDepth),
   m_dwProcessId(GetCurrentProcessId()),
   m_hDbhHelp(NULL),
   m_hProcess(GetCurrentProcess())
{}

Backtrace::~Backtrace()
{
   if (pSC != NULL)
      pSC(m_hProcess);
   if (m_hDbhHelp != NULL)
      FreeLibrary(m_hDbhHelp);
   m_hDbhHelp = NULL;
}

std::string Backtrace::GetBacktrace()
{
   Callstack();

   std::string sRet;
   std::ostringstream lineOss;
   std::ostringstream it;

   for (size_t i = 0; i < m_callStack.size(); i++)
   {
      if (i > 1)
      {
         auto frame = m_callStack[i];

         lineOss << frame.line;
         it << i - 1;

         sRet += "[bt]: (" + it.str() + ") "
            + frame.file + ":"
            + lineOss.str() + " "
            + frame.function + ":\n";

         lineOss.str("");
         it.str("");
      }
   }

   return sRet;
}

void Backtrace::Callstack()
{
   IMAGEHLP_SYMBOL64 *pSym = NULL;
   CONTEXT context;
   DWORD imageType;
   STACKFRAME64 stackFrame;
   memset(&stackFrame, 0, sizeof(stackFrame));

   LoadModule();
   GET_CURRENT_CONTEXT(context, USED_CONTEXT_FLAGS);
   InitStackFrame(context, imageType, stackFrame);

   pSym = (IMAGEHLP_SYMBOL64*)malloc(sizeof(IMAGEHLP_SYMBOL64) + BACKTRACE_MAX_NAMELEN);
   if (!pSym)
   {
      free(pSym); // not enough memory...
      return;
   }
   memset(pSym, 0, sizeof(IMAGEHLP_SYMBOL64) + BACKTRACE_MAX_NAMELEN);
   pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
   pSym->MaxNameLength = BACKTRACE_MAX_NAMELEN;

   for (int frameNum = 0; frameNum < m_maxDepth; ++frameNum)
   {
      // get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
      // if this returns ERROR_INVALID_ADDRESS (487) or ERROR_NOACCESS (998), you can
      // assume that either you are done, or that the stack is so hosed that the next
      // deeper frame could not be found.
      // CONTEXT need not to be suplied if imageTyp is IMAGE_FILE_MACHINE_I386!
      if (!pSW(imageType, this->m_hProcess, GetCurrentThread(), &stackFrame, &context, ReadProcMem, pSFTA, pSGMB, NULL))
      {
         //this->OnDbgHelpErr("StackWalk64", GetLastError(), s.AddrPC.Offset);
         //_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
         break;
      }

      CallstackEntry csEntry;
      csEntry.offset = stackFrame.AddrPC.Offset;
      csEntry.function[0] = 0;
      csEntry.file[0] = 0;
      csEntry.line = -1;
      if (stackFrame.AddrPC.Offset == stackFrame.AddrReturn.Offset)
      {
         //_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
         //this->OnDbgHelpErr("StackWalk64-Endless-Callstack!", 0, s.AddrPC.Offset);
         break;
      }
      if (stackFrame.AddrPC.Offset != 0)
      {
         // we seem to have a valid PC
         // show procedure info (SymGetSymFromAddr64())
         if (pSGSFA(this->m_hProcess, stackFrame.AddrPC.Offset, &(csEntry.offsetFromSmybol), pSym) != FALSE)
            strcpy_s(csEntry.function, pSym->Name);
         else
         {
            //_snprintf_s(buffer, BACKTRACE_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (LPVOID)addr);
            //this->OnDbgHelpErr("SymGetSymFromAddr64", GetLastError(), s.AddrPC.Offset);
         }
         GetLineFromAddr64(m_hProcess, stackFrame, csEntry);
      } // we seem to have a valid PC

      if (csEntry.offset != 0)
      {
         if (csEntry.file[0] == 0)
            strcpy_s(csEntry.file, "(NOT_AVAILABLE)");
         if (csEntry.line == -1)
            strcpy_s(csEntry.file, "(NOT_AVAILABLE)");
         if (csEntry.function[0] == 0)
            strcpy_s(csEntry.function, "(NOT_AVAILABLE)");

         m_callStack.push_back(csEntry);
         if ((std::string)csEntry.function == "main")
            return;
      }
   }

   if (NULL != pSym)
      free(pSym);
}

//! TODO: Merge 'try' and add seperate exception classes
void Backtrace::LoadModule()
{
   try
   {
      LoadDBGHELP();
   }
   catch (std::exception &e)
   {
      std::cout << "Error while initializing dbghelp.dll: " << e.what() << std::endl;
      return;
   }
   try
   {
      LoadModuleInformation(this->m_hProcess);
   }
   catch (std::exception &e)
   {
      std::cout << "Error while loading modules with psapi.dll: " << e.what() << std::endl;
      return;
   }
}

void Backtrace::LoadDBGHELP()
{
   if (m_hDbhHelp == NULL)
      m_hDbhHelp = LoadLibrary(_T("dbghelp.dll"));

   pSI = (tSI)GetProcAddress(m_hDbhHelp, "SymInitialize");
   pSC = (tSC)GetProcAddress(m_hDbhHelp, "SymCleanup");
   pSW = (tSW)GetProcAddress(m_hDbhHelp, "StackWalk64");
   pSFTA = (tSFTA)GetProcAddress(m_hDbhHelp, "SymFunctionTableAccess64");
   pSGLFA = (tSGLFA)GetProcAddress(m_hDbhHelp, "SymGetLineFromAddr64");
   pSGMB = (tSGMB)GetProcAddress(m_hDbhHelp, "SymGetModuleBase64");
   pSGSFA = (tSGSFA)GetProcAddress(m_hDbhHelp, "SymGetSymFromAddr64");
   pSLM = (tSLM)GetProcAddress(m_hDbhHelp, "SymLoadModule64");

   // https://msdn.microsoft.com/en-us/library/windows/desktop/ms681351(v=vs.85).aspx
   if (pSI(m_hProcess, NULL, FALSE) == FALSE)
   {
      FreeLibrary(m_hDbhHelp);
      m_hDbhHelp = NULL;
      pSC = NULL;
      throw std::exception("Could not initialize symbols..");
   }
}

void Backtrace::LoadModuleInformation(HANDLE hProcess)
{
   typedef struct _MODULEINFO {
      LPVOID   lpBaseOfDll;
      DWORD    SizeOfImage;
      LPVOID   EntryPoint;
   } *LPMODULEINFO;
   _MODULEINFO moduleInfo;

   typedef BOOL(__stdcall *tEPM)(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded); tEPM pEPM;
   typedef DWORD(__stdcall *tGMFNE)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize); tGMFNE pGMFNE;
   typedef BOOL(__stdcall *tGMI)(HANDLE hProcess, HMODULE hModule, LPMODULEINFO pmi, DWORD nSize); tGMI pGMI;

   HINSTANCE hPsapi;

   HMODULE *hMods = 0;
   const SIZE_T BUFFER_LEN = 8096;

   hPsapi = LoadLibrary(_T("psapi.dll"));
   if (hPsapi == NULL)
      throw std::exception("Could not load PSAPI library..");

   pEPM = (tEPM)GetProcAddress(hPsapi, "EnumProcessModules");
   pGMFNE = (tGMFNE)GetProcAddress(hPsapi, "GetModuleFileNameExA");
   pGMI = (tGMI)GetProcAddress(hPsapi, "GetModuleInformation");

   if ((pEPM == NULL) || (pGMFNE == NULL) || (pGMI == NULL))
   {
      FreeLibrary(hPsapi);
      return throw std::exception("Could not load all functions..");
   }

   hMods = (HMODULE*)malloc(sizeof(HMODULE) * (BUFFER_LEN / sizeof HMODULE));
   if (hMods == NULL)
   {
      if (hPsapi != NULL) FreeLibrary(hPsapi);
      if (hMods != NULL) free(hMods);
      throw std::exception("Not enough memory..");
   }

   DWORD cbNeeded; // just to cover interface
   if (!pEPM(hProcess, hMods, BUFFER_LEN, &cbNeeded))
   {
      if (hPsapi != NULL) FreeLibrary(hPsapi);
      if (hMods != NULL) free(hMods);
      throw std::exception("Could not enumerate modules..");
   }

   pGMI(hProcess, hMods[0], &moduleInfo, sizeof moduleInfo); // base address, size

   char modulePath[sizeof(char) * BUFFER_LEN];
   modulePath[0] = 0;
   pGMFNE(hProcess, hMods[0], (LPSTR)modulePath, BACKTRACE_MAX_NAMELEN); // image file function

   std::string strModulePath = modulePath;
   if (LoadModule(hProcess, strModulePath, (DWORD64)moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage) != ERROR_SUCCESS)
      throw std::exception("Could not load module information..");

   if (hPsapi != NULL) FreeLibrary(hPsapi);
   if (hMods != NULL) free(hMods);
}

DWORD Backtrace::LoadModule(HANDLE hProcess, const std::string& img, DWORD64 baseAddr, DWORD size)
{
   DWORD result = ERROR_SUCCESS;
   if (pSLM(hProcess, NULL, (PSTR)img.c_str(), NULL, baseAddr, size) == ERROR_SUCCESS)
      result = GetLastError();

   return result;
}