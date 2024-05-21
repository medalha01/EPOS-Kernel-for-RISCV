// EPOS RISC-V Interrupt Controller Initialization

#include <machine/ic.h>
#include <machine/timer.h>

__BEGIN_SYS

void IC::init()
{
    db<Init, IC>(TRC) << "IC::init()" << endl;

	// will be reenabled at Thread::init() by Context::load()
    assert(CPU::int_disabled()); 

	// will be enabled on demand as handlers are registered
    disable(); 

	if (CPU::is_bootstrap()) 
	{
		// Set all exception handlers to exception()
		for(Interrupt_Id i = 0; i < EXCS; i++)
		{
			_int_vector[i] = &exception;
		}

		// Set all interrupt handlers to int_not()
		for(Interrupt_Id i = EXCS; i < INTS; i++)
		{
			_int_vector[i] = &int_software;
		}
	}

	// This ensures that, before enabling PLIC interrupts, the interrupt
	// handler vector is correctly set up by the bootstrap core.
	CPU::smp_barrier();

	IC::enable(INT_PLIC);
	PLIC::threshold(0); // set the threshold to 0 so all enabled external
						// interrupts will be dispatched
}

__END_SYS
