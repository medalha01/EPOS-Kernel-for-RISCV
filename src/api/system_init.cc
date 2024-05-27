// EPOS System Initialization

#include <system.h>
#include <time.h>
#include <process.h>

__BEGIN_SYS

void System::init()
{
    if (CPU::is_bootstrap())
    {
        if (Traits<Alarm>::enabled)
		{
            Alarm::init();
		}
    }
	
	// Ensures the alarm timer is properly allocated by the bootstrap core.
	CPU::smp_barrier();

    if (Traits<Thread>::enabled) 
	{
        Thread::init();
	}
}

__END_SYS
