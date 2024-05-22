// EPOS System Initialization

#include <system.h>
#include <time.h>
#include <process.h>

__BEGIN_SYS

void System::init()
{
    if(Traits<Alarm>::enabled && CPU::is_bootstrap())
	{
        Alarm::init();
	}

	// Guarantees that the timer has been initialized in memory before proceeding
	db<Thread>(WRN) << "@@@SYSINIT ANTES do barrier" << endl;
	CPU::smp_barrier();
	db<Thread>(WRN) << "@@@SYSINIT depois do barrier" << endl;

    if(Traits<Thread>::enabled)
	{
        Thread::init();
	}
}

__END_SYS
