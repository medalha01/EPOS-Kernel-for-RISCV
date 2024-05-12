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
    db<Init, Thread>(TRC) << "Thread::init()" << endl;

    CPU::smp_barrier();

    // If EPOS is a library, then adjust the application entry point to __epos_app_entry, which will directly call main().
    // In this case, _init will have already been called, before Init_Application to construct MAIN's global objects.
    if (CPU::is_bootstrap())
    {
        typedef int(Main)();

        Main *main = reinterpret_cast<Main *>(__epos_app_entry);

        new (SYSTEM) Task(main);

        // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
        new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);
    }
    CPU::smp_barrier();

    db<Thread>(TRC) << "GRITA TEU NOME SATANAS\n"
                    << CPU::id() << "\n"
                    << endl;

    // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
    new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);

    CPU::smp_barrier();

    db<Thread>(TRC) << "GRITA TEU NOME FAZUL\n"
                    << CPU::id() << "\n"
                    << endl;

    Criterion::init();

    // The installation of the scheduler timer handler does not need to be done after the
    // creation of threads, since the constructor won't call reschedule() which won't call
    // dispatch that could call timer->reset()
    // Letting reschedule() happen during thread creation is also harmless, since MAIN is
    // created first and dispatch won't replace it nor by itself neither by IDLE (which
    // has a lower priority)
    if (Criterion::timed && (CPU::is_bootstrap()))
        _timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);

    // No more interrupts until we reach init_end

    db<Thread>(TRC) << "GRITA TEU NOME JOVENAS\n"
                    << CPU::id() << "\n"
                    << endl;
    CPU::int_disable();
    CPU::smp_barrier();
}

__END_SYS
