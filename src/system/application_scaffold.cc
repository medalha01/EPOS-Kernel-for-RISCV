// EPOS Application Scaffold and Application Component Implementation

#include <system.h>

__BEGIN_SYS

// Application class attributes
char Application::_preheap[];
Heap *Application::_heap;

__END_SYS

extern "C"
{
    //static _UTIL::Simple_Spin _heap_spin;
    //void _lock_heap() { _heap_spin.acquire(); }
    //void _unlock_heap() { _heap_spin.release(); }
}
__BEGIN_API

// Global objects
__USING_UTIL
OStream cout;
OStream cerr;

__END_API
