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
            Alarm::init();
    }
    if (Traits<Thread>::enabled) 
	{
		db<Thread>(WRN) << "antes do thread::init\n " <<endl;
        Thread::init();
	}
}

__END_SYS
