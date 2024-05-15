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
		IC::int_vector(IC::INT_RESCHEDULER, int_rescheduler);
	}

	db<Thread>(TRC) << "Core " << CPU::id() << ": Reached first barrier in Thread::init()" << endl;
	CPU::smp_barrier();

	db<Thread>(TRC) << "Core " << CPU::id() << ": Passed first barrier in Thread::init()" << endl;

	if (Traits<Machine>::multi)
	{
		IC::enable(IC::INT_RESCHEDULER);
	}

	Criterion::init();
	db<Thread>(TRC) << "Core " << CPU::id() << ": Criterion initialized in Thread::init()" << endl;

	typedef int(Main)();

	if (CPU::is_bootstrap())
	{
		db<Thread>(TRC) << "Bootstrap processor initializing MAIN thread" << endl;

		Main *main = reinterpret_cast<Main *>(__epos_app_entry);

		db<Thread>(TRC) << "Core " << CPU::id() << ": Creating MAIN thread" << endl;
		new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN), main);
		// Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)

		db<Thread>(TRC) << "Core " << CPU::id() << ": MAIN thread created" << endl;
	}

	db<Thread>(TRC) << "Core " << CPU::id() << ": Reached second barrier in Thread::init()" << endl;
	CPU::smp_barrier();
	db<Thread>(TRC) << "Core " << CPU::id() << ": Passed second barrier in Thread::init()" << endl;

	new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);

	db<Thread>(TRC) << "Core " << CPU::id() << ": IDLE thread created" << endl;
	CPU::smp_barrier();
	db<Thread>(TRC) << "Core " << CPU::id() << ": Passed third barrier after IDLE creation" << endl;
	// The installation of the scheduler timer handler does not need to be done after the
	// creation of threads, since the constructor won't call reschedule() which won't call
	// dispatch that could call timer->reset()
	// Letting reschedule() happen during thread creation is also harmless, since MAIN is
	// created first and dispatch won't replace it nor by itself neither by IDLE (which
	// has a lower priority)
	if (Criterion::timed && CPU::is_bootstrap())
	{
		db<Thread>(TRC) << "Core " << CPU::id() << ": Setting up scheduler timer with QUANTUM = " << QUANTUM << endl;
		_timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);
	}

	db<Thread>(TRC) << "Core " << CPU::id() << ": Reached final barrier in Thread::init()" << endl;
	CPU::smp_barrier();
	db<Thread>(TRC) << "Core " << CPU::id() << ": Passed final barrier in Thread::init()" << endl;
	// No more interrupts until we reach init_end

	CPU::int_disable();
	_not_booting = true;

	db<Thread>(TRC) << "Thread::init() completed" << endl;
}

__END_SYS
