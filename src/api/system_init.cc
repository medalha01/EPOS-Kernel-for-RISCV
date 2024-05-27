// EPOS System Initialization

#include <system.h>
#include <time.h>
#include <process.h>

__BEGIN_SYS

void System::init()
{
	_print("system_init\n");

    if (CPU::is_bootstrap())
    {
        if (Traits<Alarm>::enabled)
		{
            Alarm::init();
		}
    }
	
	CPU::smp_barrier();

    if (Traits<Thread>::enabled) 
	{
        Thread::init();
	}
}

__END_SYS
