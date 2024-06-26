// EPOS RISC V Initialization

#include <machine.h>

__BEGIN_SYS

void Machine::pre_init(System_Info *si)
{
    CPU::tvec(CPU::INT_DIRECT, &IC::entry);

    if (CPU::is_bootstrap())
    {
        Display::init();
    }

	// Other cores wait for the bootstrap to finish setting up the Display.
	CPU::smp_barrier();
}

void Machine::init()
{
    db<Init, Machine>(TRC) << "Machine::init()" << endl;

    if (Traits<IC>::enabled)
        IC::init();

    if (Traits<Timer>::enabled)
        Timer::init();
}

__END_SYS
