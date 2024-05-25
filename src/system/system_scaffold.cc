// EPOS System Scaffold and System Component Implementation

#include <utility/ostream.h>
#include <utility/heap.h>
#include <machine.h>
#include <memory.h>
#include <process.h>
#include <system.h>

extern char __boot_time_system_info[];

__BEGIN_SYS

// Global objects
// These objects might be reconstructed several times in multicore configurations, so their constructors must be stateless!
OStream kout;
OStream kerr;

// System class attributes
System_Info *System::_si = (Memory_Map::SYS_INFO != Memory_Map::NOT_USED) ? reinterpret_cast<System_Info *>(Memory_Map::SYS_INFO) : reinterpret_cast<System_Info *>(&__boot_time_system_info);
char System::_preheap[];
Segment *System::_heap_segment;
Heap *System::_heap;

__END_SYS

// Bindings
extern "C"
{
    __USING_SYS;

    // Libc legacy
    void _exit(int s)
    {
        Thread::exit(s);
        for (;;)
            ;
    }
    void __exit() { _exit(CPU::fr()); } // must be handled by the Page Fault handler for user-level tasks
    void __cxa_pure_virtual() { db<void>(ERR) << "Pure Virtual method called!" << endl; }

    // Utility-related methods that differ from kernel and user space.
    // OStream
    // Utility-related methods that differ from kernel and user space.
    // OStream
    void _print(const char *s) { Display::puts(s); }
    static volatile int _print_lock = -1;
    void _print_preamble()
    {
        if (Traits<Machine>::multi)
        {
            static char tag[] = "<0>: ";

            int me = CPU::id();
            int last = CPU::cas(_print_lock, -1, me);
            for (int i = 0, owner = last;
                 (Traits<System>::hysterically_debugged || (i < 100)) &&
                 (owner != me);
                 i++, owner = CPU::cas(_print_lock, -1, me))
              ;
            if (last != me)
            {
                tag[1] = '0' + CPU::id();
                _print(tag);
            }
        }
    }

    void _print_trailler(bool error)
    {
        if (Traits<Machine>::multi)
        {
            static char tag[] = " :<0>";

            if (_print_lock != -1)
            {
                tag[3] = '0' + CPU::id();
                _print(tag);

                _print_lock = -1;
            }
        }
        if (error) 
		{
            Machine::panic(__FILE__, __LINE__);
		}
    }

    static Spin _heap_lock;
    void _lock_heap() { Thread::lock(&_heap_lock); }
    void _unlock_heap() { Thread::unlock(&_heap_lock); }
}
