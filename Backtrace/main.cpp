#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include "Backtrace.h"

void Func5() { Backtrace bt; bt.ShowCallstack(); std::cout << bt.ToString(); }
void Func4() { Func5(); }
void Func3() { Func4(); }
void Func2() { Func3(); }
void TestCurrentThread() { Func2(); }

int main()
{
   TestCurrentThread();

   return 0;
}
