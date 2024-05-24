// EPOS Thread Initialization

#include <machine/timer.h>
#include <machine/ic.h>
#include <system.h>
#include <process.h>

__BEGIN_SYS

extern "C" { void __epos_app_entry(); }

void Thread::init()
{
    db<Init, Thread>(TRC) << "Thread::init()" << endl;

    Criterion::init();

	if (CPU::is_bootstrap()) 
	{
		typedef int (Main)();

		// If EPOS is a library, then adjust the application entry point to __epos_app_entry, which will directly call main().
		// In this case, _init will have already been called, before Init_Application to construct MAIN's global objects.
		Main * main = reinterpret_cast<Main *>(__epos_app_entry);

		new (SYSTEM) Task(main);
	}

	// Waits for there to be a main, otherwise initialization of the IDLE threads might be problematic.
	// TODO: @arthur find out why
	db<Thread>(WRN) << "@@@THREADINIT antes -- medio" << endl;
	CPU::smp_barrier();
	//db<Thread>(WRN) << "@@@THREADINIT DEPOIS -- medio" << endl;

    // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
    new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);

    // The installation of the scheduler timer handler does not need to be done after the
    // creation of threads, since the constructor won't call reschedule() which won't call
    // dispatch that could call timer->reset()
    // Letting reschedule() happen during thread creation is also harmless, since MAIN is
    // created first and dispatch won't replace it nor by itself neither by IDLE (which
    // has a lower priority)
    if(Criterion::timed && CPU::is_bootstrap()) 
	{
        _timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);
	}

	// Guarantees that the Scheduler_Timer is ready for all the cores.
	db<Thread>(WRN) << "@@@THREADINIT antes -- final" << endl;
	CPU::smp_barrier();
	db<Thread>(WRN) << "@@@THREADINIT DEPOIS -- final" << endl;

	// Allow for software interrupts (used for inter-core communication)
	IC::enable(IC::INT_RESCHEDULER);
	
    // No more interrupts until we reach init_end
    CPU::int_disable();
}

__END_SYS
