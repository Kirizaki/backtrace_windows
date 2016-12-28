#include "Backtrace.h"

void Func5() { Backtrace bt; std::cout << bt.GetBacktrace(); }
void Func4() { Func5(); }
void Func3() { Func4(); }
void Func2() { Func3(); }
void Func1() { Func2(); }

int main()
{
   Func1();

   return 0;
}
