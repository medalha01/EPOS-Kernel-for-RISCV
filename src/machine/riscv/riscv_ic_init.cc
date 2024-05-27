// EPOS RISC-V Interrupt Controller Initialization

#include <machine/ic.h>
#include <machine/timer.h>

__BEGIN_SYS

void IC::init()
{
    db<Init, IC>(TRC) << "IC::init()" << endl;

    assert(CPU::int_disabled()); // will be reenabled at Thread::init() by Context::load()

    disable(); // will be enabled on demand as handlers are registered

    if (CPU::is_bootstrap())
    {
        // Set all exception handlers to exception()
        for (Interrupt_Id i = 0; i < EXCS; i++)
		{
            _int_vector[i] = &exception;
		}

        // Set all interrupt handlers to int_not()
        for (Interrupt_Id i = EXCS; i < INTS; i++)
		{
            _int_vector[i] = &int_not;
		}
    }
    
	// It is paramount that all cores wait for the bootstrap to initialize the interrupt vector,
	// otherwise, bad things would happen if an interrupt were to occur, and the handler function
	// has not yet been put into its correct position into the _int_vector - undefined behaviour.
	CPU::smp_barrier();

	IC::enable(INT_PLIC);
    PLIC::threshold(0); // set the threshold to 0 so all enabled external interrupts will be dispatched
}

__END_SYS
