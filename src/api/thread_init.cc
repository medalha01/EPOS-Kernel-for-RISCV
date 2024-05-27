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
	if (CPU::is_smp() && CPU::is_bootstrap())
	{
		// This is utilized only in multi queue schedulers;
		IC::int_vector(IC::INT_RESCHEDULER, int_rescheduler);
	}

	// Wait for the bootstrap core to properly set up the int_rescheduler in the vector.
	// Thus, proper initialization is needed before going into the constructors, as they
	// use inter-core interrupts.
	CPU::smp_barrier();

	Criterion::init(); // does nothing :P

	typedef int(Main)();

	if (CPU::is_bootstrap())
	{
		// Initialize the core-priority lookup table, 
		// useful for figuring out where to send interrupts.
		Thread::_cpu_lookup_table = CpuLookupTable();

		Main *main = reinterpret_cast<Main *>(__epos_app_entry);
		new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN), main);
	}

	// Waits for the main thread to be initialized before going into the IDLEs. 
	// This is important because of the checks in Thread::idle - if there are only IDLEs,
	// and no MAIN, then the _thread_count will be the same as the CPU::cores(), and thus
	// we will potentially be rebooting the machine right away. Additionally, the constructor
	// of the Thread expects MAIN to be the first to properly initialize the CPU Lookup Table.
	CPU::smp_barrier();

	// Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
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

	// Waits for the tiemr to be initialized, otherwise there might be scheduling problems.
	CPU::smp_barrier();
	
	// No more interrupts until we reach init_end
	CPU::int_disable();
	_not_booting = true;

	if (CPU::is_smp())
	{
		IC::enable(IC::INT_RESCHEDULER);
	}

	db<Thread>(TRC) << "Thread::init() completed" << endl;
}

__END_SYS
