// EPOS Initializer End

#include <architecture.h>
#include <system.h>
#include <process.h>

__BEGIN_SYS

// This class purpose is simply to define a well-known ending point for the initialization of the system.
// It activates the first application thread (usually main()).
// It must be linked first so init_end becomes the last constructor in the global's constructor list.

class Init_End
{
public:
    Init_End()
    {
		_print("init_end\n");
        db<Thread>(TRC) << "Start of Init_End()" << endl;

        CPU::smp_barrier();

        if (!Traits<System>::multithread)
        {
            CPU::int_enable();
            return;
        }

        if (CPU::is_bootstrap())
        {
            if (Memory_Map::BOOT_STACK != Memory_Map::NOT_USED)
            {
                MMU::free(Memory_Map::BOOT_STACK, MMU::pages(Traits<Machine>::STACK_SIZE * CPU::cores()));
            }
        }

        CPU::smp_barrier();

        // Thread::self() and Task::self() can be safely called after the construction of MAIN
        // even if no reschedule() was called (running is set by the Scheduler at each insert())
        // It will return MAIN for CPU0 and IDLE for the others

        Thread *first = Thread::self();

        CPU::smp_barrier();

        // Interrupts have been disabled at Thread::init() and will be reenabled by CPU::Context::load()
        // but we first reset the timer to avoid getting a time interrupt during load()
        if (Traits<Timer>::enabled)
        {
            Timer::reset();
        }

        first->_context->load();
    }
};

Init_End init_end;

__END_SYS
