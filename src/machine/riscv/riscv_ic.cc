// EPOS RISC-V IC Mediator Implementation

#include <architecture.h>
#include <machine/machine.h>
#include <machine/ic.h>
#include <machine/timer.h>
#include <process.h>

extern "C"
{
  static void print_context(bool push);
  static void print_entry();
  static void print_end();
}

__BEGIN_SYS

PLIC::Reg32 PLIC::_claimed;
IC::Interrupt_Handler IC::_int_vector[IC::INTS];
CLINT::Reg64 IC::irq_exec_highest_time = 0;
CLINT::Reg64 IC::irq_iter_sum = 0;
int IC::n_measures = 0;

void IC::entry()
{
  // Log SP and EPC at ISR entry
  if (Traits<IC>::debugged)
    print_entry();

  // Save context into the stack
  CPU::Context::push(true);
  unsigned int start = CLINT::mtime();

  if (Traits<IC>::hysterically_debugged)
    print_context(true);

  dispatch(start);

  if (Traits<IC>::hysterically_debugged)
    print_context(false);

  // Restore context from the stack
  CPU::Context::pop(true);

  if (Traits<IC>::debugged)
    print_end();

  CPU::iret();
}

void IC::dispatch(unsigned int start)
{
  Interrupt_Id id = int_id();
  if ((id != INT_SYS_TIMER) || Traits<IC>::hysterically_debugged)
    db<IC, System>(TRC) << "IC::dispatch(i=" << id << ") [sp=" << CPU::sp() << "]" << endl;

  if (id == INT_SYS_TIMER)
  {
    if (supervisor)
      CPU::ecall(); // we can't clear CPU::sipc(CPU::STI) in supervisor mode,
                    // so let's ecall int_m2s to do it for us
    else
      Timer::reset(); // MIP.MTI is a direct logic on (MTIME == MTIMECMP) and
                      // reseting the Timer seems to be the only way to clear
                      // it
  }

  _int_vector[id](id);

  CLINT::Reg64 current_irq_exec_time = CLINT::mtime() - start;
  IC::update_if_highest_irq_time(current_irq_exec_time);

  db<IC, System>(WRN) << "highest time: " << IC::irq_exec_highest_time << endl;

  if (id >= EXCS) 
    CPU::fr(0);   // tell CPU::Context::pop(true) not to increment PC since it is automatically incremented for hardware interrupts
}

void IC::int_not(Interrupt_Id id)
{
  db<IC>(WRN) << "IC::int_not(i=" << id << ")" << endl;
  if (Traits<Build>::hysterically_debugged)
    Machine::panic();
}

void IC::exception(Interrupt_Id id)
{
  CPU::Log_Addr sp = CPU::sp();
  CPU::Log_Addr epc = CPU::epc();
  CPU::Reg status = CPU::status();
  CPU::Reg cause = CPU::cause();
  CPU::Log_Addr tval = CPU::tval();
  Thread *thread = Thread::self();

  db<IC>(WRN) << "IC::Exception(" << id << ") => {" << hex << "thread=" << thread << ",sp=" << sp << ",status=" << status << ",cause=" << cause << ",epc=" << epc << ",tval=" << tval << "}" << dec;

  switch (id)
  {
  case CPU::EXC_IALIGN: // instruction address misaligned
    db<IC, System>(WRN) << " => unaligned instruction";
    break;
  case CPU::EXC_IFAULT: // instruction access fault
    db<IC, System>(WRN) << " => instruction protection violation";
    break;
  case CPU::EXC_IILLEGAL: // illegal instruction
    db<IC, System>(WRN) << " => illegal instruction";
    break;
  case CPU::EXC_BREAK: // break point
    db<IC, System>(WRN) << " => break point";
    break;
  case CPU::EXC_DRALIGN: // load address misaligned
    db<IC, System>(WRN) << " => unaligned data read";
    break;
  case CPU::EXC_DRFAULT: // load access fault
    db<IC, System>(WRN) << " => data protection violation (read)";
    break;
  case CPU::EXC_DWALIGN: // store/AMO address misaligned
    db<IC, System>(WRN) << " => unaligned data write";
    break;
  case CPU::EXC_DWFAULT: // store/AMO access fault
    db<IC, System>(WRN) << " => data protection violation (write)";
    break;
  case CPU::EXC_ENVU: // ecall from user-mode
  case CPU::EXC_ENVS: // ecall from supervisor-mode
  case CPU::EXC_ENVH: // reserved
  case CPU::EXC_ENVM: // reserved
    db<IC, System>(WRN) << " => bad ecall";
    break;
  case CPU::EXC_IPF: // Instruction page fault
    db<IC, System>(WRN) << " => instruction page fault";
    break;
  case CPU::EXC_DRPF: // load page fault
  case CPU::EXC_RES:  // reserved
  case CPU::EXC_DWPF: // store/AMO page fault
    db<IC, System>(WRN) << " => data page fault";
    break;
  default:
    int_not(id);
    break;
  }

  db<IC, System>(WRN) << endl;

  if (Traits<Build>::hysterically_debugged)
    db<IC, System>(ERR) << "Exception stopped execution due to hysterically debugging!" << endl;
  else
  {
    db<IC, Machine>(WRN) << "The running thread will now be terminated!" << endl;
    Thread::exit(-1);
  }

  CPU::fr(4); // since exceptions do not increment PC, tell CPU::Context::pop(true) to perform PC = PC + 4 on return
}

__END_SYS

static void print_context(bool push)
{
  __USING_SYS
  db<IC, System>(TRC) << "IC::entry:" << (push ? "push" : "pop") << ":ctx=" << *static_cast<CPU::Context *>(CPU::sp() + 3 * sizeof(CPU::Reg) + (push ? sizeof(CPU::Context) : 0)) << endl; // 3 words for function's stack frame
  CPU::fr(0);
}

static void print_entry()
{
  __USING_SYS
  db<IC>(TRC) << "ISR Entry [SP=" << hex << CPU::sp() << ", EPC=" << hex << CPU::epc() << "]" << dec;
}

static void print_end()
{
  __USING_SYS
  db<IC>(TRC) << "ISR Exit [SP=" << hex << CPU::sp() << ", EPC=" << hex << CPU::epc() << "]" << dec;
}
