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

void Thread::init()
{
	db<Init, Thread>(TRC) << "Thread::init() started" << endl;

	if (Traits<Machine>::multi && CPU::is_bootstrap())
	{
		// This is utilized only in multi queue schedulers;
		//IC::int_vector(IC::INT_RESCHEDULER, int_rescheduler);
	}

	CPU::smp_barrier();

	Criterion::init();

	typedef int(Main)();

	if (CPU::is_bootstrap())
	{
		// TODO: aqui

		Main *main = reinterpret_cast<Main *>(__epos_app_entry);
		new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN), main);
		// Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
	}

	CPU::smp_barrier();

	new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);

	// The installation of the scheduler timer handler does not need to be done after the
	// creation of threads, since the constructor won't call reschedule() which won't call
	// dispatch that could call timer->reset()
	// Letting reschedule() happen during thread creation is also harmless, since MAIN is
	// created first and dispatch won't replace it nor by itself neither by IDLE (which
	// has a lower priority)
	if (Criterion::timed && CPU::is_bootstrap())
	{
		_timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);
	}

	CPU::smp_barrier();
	
	// No more interrupts until we reach init_end
	CPU::int_disable();
	_not_booting = true;

	if (Traits<Machine>::multi)
	{
		IC::enable(IC::INT_RESCHEDULER);
	}

	db<Thread>(TRC) << "Thread::init() completed" << endl;
}

__END_SYS
