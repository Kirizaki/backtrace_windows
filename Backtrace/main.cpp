#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#if _MSC_VER >= 1300
#include <Tlhelp32.h>
#endif

#include "Backtrace.h"

void Func5() { Backtrace bt; bt.ShowCallstack(); }
void Func4() { Func5(); }
void Func3() { Func4(); }
void Func2() { Func3(); }
void TestCurrentThread() { Func2(); }

//int _tmain()
int main()
{
   TestCurrentThread();

   return 0;
}
