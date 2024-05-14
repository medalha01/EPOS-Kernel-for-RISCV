// EPOS Thread Initialization

#include <machine/timer.h>
#include <machine/ic.h>
#include <system.h>
#include <process.h>

__BEGIN_SYS

extern "C"
{
    void __epos_app_entry();
}

volatile bool isInitReady = false;
void Thread::init()
{
    db<Init, Thread>(WRN) << "no Thread::init()" << endl;

	if (Traits<Machine>::multi && CPU::is_bootstrap())
	{
		IC::int_vector(IC::INT_RESCHEDULER, int_rescheduler);
	}

	db<Thread>(WRN) << "---core = " << CPU::id() << "thred_init antes da 1\n\n" << endl;

	CPU::smp_barrier();

	// NOTE: ambos os cores passam dessa barreira

	db<Thread>(WRN) << "---core = " << CPU::id() << "thread_init dps da 1\n" << endl;

	if (Traits<Machine>::multi) { 
		IC::enable(IC::INT_RESCHEDULER); 
	}

    Criterion::init();

	db<Thread>(WRN) << "---core = " << CPU::id() 
		<< ", dps criterion\n" << endl;

	// NOTE: ambos passam daqui

    // If EPOS is a library, then adjust the application entry point to __epos_app_entry, which will directly call main().
    // In this case, _init will have already been called, before Init_Application to construct MAIN's global objects.
	typedef int(Main)();

    if (CPU::is_bootstrap())
    {
		db<Thread>(WRN) << "is_bootstrap no if\n\n" << endl;

        Main *main = reinterpret_cast<Main *>(__epos_app_entry);

		db<Thread>(WRN) << "INIT_MAIN BOOOTSTRAP, core = " << CPU::id() << endl;
		new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::MAIN), main);

		db<Thread>(WRN) << "DPS DA MAIN BOOTSTRAP, core = " << CPU::id() << endl;

        // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
    }

	db<Thread>(WRN) << "------------smp_barrier do thread_init, core = " 
		<< CPU::id() << endl;
    CPU::smp_barrier();
	db<Thread>(WRN) << "------------DEPOIS do smp_barrier do thread_init, core = " 
		<< CPU::id() << endl;
	
    // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
    new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);

	db<Thread>(WRN) << "DEPOIS DE INIT IDLE\n" << endl;

    CPU::smp_barrier();

	db<Thread>(WRN) << "DPS DA IDLE IDLE IDLE\n" << endl;
    // The installation of the scheduler timer handler does not need to be done after the
    // creation of threads, since the constructor won't call reschedule() which won't call
    // dispatch that could call timer->reset()
    // Letting reschedule() happen during thread creation is also harmless, since MAIN is
    // created first and dispatch won't replace it nor by itself neither by IDLE (which
    // has a lower priority)
    if (Criterion::timed && (CPU::is_bootstrap()))
	{
        _timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);
	}

	db<Thread>(WRN) << "ANTES DA BARREIRA NO FINAL DO THREAD_INIT" << endl;
	CPU::smp_barrier();
	db<Thread>(WRN) << "thread_init dps do _timer\n\n" << endl;

    // No more interrupts until we reach init_end
    CPU::int_disable();
}

__END_SYS
