// EPOS RISC-V Mediator Declarations

#ifndef __riscv_machine_h
#define __riscv_machine_h

#include <architecture.h>
#include <machine/machine.h>
#include <machine/ic.h>
#include <machine/display.h>
#include <system/info.h>
#include <system/memory_map.h>
#include <system.h>


__BEGIN_SYS

class Machine: private Machine_Common
{
    friend class Setup;
    friend class Init_Begin;
    friend class Init_System;

private:
    static const bool supervisor = Traits<Machine>::supervisor;

public:
    Machine() {}

    using Machine_Common::delay;
    using Machine_Common::clear_bss;

    static void panic();
    static void reboot();
    static void poweroff();

	static const UUID & uuid() 
	{
		static const unsigned char id[8] = { '1', '2', '3', '4', '5', '6', '7', '\0' };
		/*return System::info()->bm.uuid;*/ 
		return id;
	}

private:
    static void pre_init(System_Info * si);
    static void init();
};

__END_SYS

#endif
