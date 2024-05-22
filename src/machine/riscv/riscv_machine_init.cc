// EPOS RISC V Initialization

#include <machine.h>

__BEGIN_SYS

void Machine::pre_init(System_Info * si)
{
    CPU::tvec(CPU::INT_DIRECT, &IC::entry);

	if (CPU::is_bootstrap())
	{
		Display::init();
	}

	db<Thread>(WRN) << "@@@MACHINEPREINIT ANTES do barrier " << endl;
	CPU::smp_barrier();
	db<Thread>(WRN) << "@@@MACHINEPREINIT depois do barrier " << endl;

    db<Init, Machine>(TRC) << "Machine::pre_init()" << endl;
}


void Machine::init()
{
    db<Init, Machine>(TRC) << "Machine::init()" << endl;

    if(Traits<IC>::enabled)
	{
		db<Thread>(WRN) << "@@@__ICINIT - ANTES ANTES ANTES DO IC INIT" << endl;
        IC::init();
	}

    if(Traits<Timer>::enabled)
	{
        Timer::init();
	}
}

__END_SYS
