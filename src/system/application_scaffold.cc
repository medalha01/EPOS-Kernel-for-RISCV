// EPOS Application Scaffold and Application Component Implementation

#include <system.h>
#include <process.h>

__BEGIN_SYS

// Application class attributes
char Application::_preheap[];
Heap * Application::_heap;

extern "C"
{
    static _UTIL::Simple_Spin _heap_spin;

    void _lock_heap() 
	{
		//db<Thread>(WRN) << "\n\nheap --- @@@lock heap antes" << endl;
		//Thread::lock(&_heap_spin);
		_heap_spin.acquire(); 
		//db<Thread>(WRN) << "\nheap --- @@@lock heap dps\n" << endl;
	}
    void _unlock_heap() 
	{
		//db<Thread>(WRN) << "\n\nheap --- @@@unlock heap antes" << endl;
		_heap_spin.release(); 
		//db<Thread>(WRN) << "\nheap --- @@@unlock heap dps\n" << endl;
		//Thread::unlock(&_heap_spin);
	}
}

__END_SYS

__BEGIN_API

// Global objects
__USING_UTIL
OStream cout;
OStream cerr;

__END_API

