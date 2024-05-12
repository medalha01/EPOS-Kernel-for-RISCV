// EPOS PC Mediator Initialization

#include <machine.h>

__BEGIN_SYS
extern OStream kout, kerr;

void Machine::pre_init(System_Info *si)
{

    if (CPU::is_bootstrap())
        Display::init();

    db<Init, Machine>(TRC) << "Machine::pre_init()" << endl;
}

void Machine::init()
{
    db<Init, Machine>(TRC) << "Machine::init()" << endl;

    if (Traits<IC>::enabled)
        IC::init();

    if (Traits<Timer>::enabled)
        Timer::init();

    if (Traits<PCI>::enabled)
        PCI::init();

#ifdef __SCRATCHPAD_H
    if (Traits<Scratchpad>::enabled)
        Scratchpad::init();
#endif

#ifdef __KEYBOARD_H
    if (Traits<Keyboard>::enabled)
        Keyboard::init();
#endif

#ifdef __FPGA_H
    if (Traits<FPGA>::enabled)
        FPGA::init();
#endif
}

__END_SYS
