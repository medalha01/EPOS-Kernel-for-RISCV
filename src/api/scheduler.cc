// EPOS CPU Scheduler Component Implementation

#include <process.h>
#include <time.h>

__BEGIN_SYS

// The following Scheduling Criteria depend on Alarm, which is not available at scheduler.h
template <typename... Tn>
FCFS::FCFS(int p, Tn &...an) : Priority((p == IDLE) ? IDLE : Alarm::elapsed()) {}

EDF::EDF(const Microsecond &d, const Microsecond &p, const Microsecond &c, unsigned int) : Real_Time_Scheduler_Common(Alarm::ticks(d), Alarm::ticks(d), p, c) {}

void EDF::update()
{
    if ((_priority >= PERIODIC) && (_priority < APERIODIC))
        _priority = Alarm::elapsed() + _deadline;
}

LLF::LLF(const Microsecond &_deadline, const Microsecond &_period, const Microsecond &_capacity, unsigned int) : Real_Time_Scheduler_Common(Alarm::ticks(_deadline - _capacity), Alarm::ticks(_deadline), _period, Alarm::ticks(_capacity)) {}

void LLF::reset_init_time()
{
    _computed_time = 0;
    _init_time = Alarm::elapsed();
    _start_of_computation = 0;
    _locked = false;
}

void LLF::start_calculation()
{
    _start_of_computation = Alarm::elapsed();
}

void LLF::set_calculated_time()
{

    _computed_time = Alarm::elapsed() - _start_of_computation;
}

void LLF::update()
{
    if ((_priority > MAIN) && (_priority < IDLE) && (!_locked)) // Não podemos dar update na IDLE, se não o avião cai.
    {
        _priority = _deadline + _init_time - _capacity + _computed_time;
    }
}

void LLF::enter_critical()
{
    if (_locked)
    {
        db<Thread>(WRN) << "Entering a critical zone while inside a critical zone" << endl;
    }
    if ((_priority > MAIN) && (_priority < IDLE) && !_locked) // Não podemos dar update na IDLE, se não o avião cai.
    {
        db<Thread>(WRN) << "Thread entering critical zone priority" << endl;

        _priority = 1;
        _locked = true;
    }
}

void LLF::leave_critical()
{
    if (!_locked)
    {
        db<Thread>(WRN) << "Trying to unlock a already unlock Thread" << endl;
    }
    if ((_priority > MAIN) && (_priority < IDLE) && _locked) // Não podemos dar update na IDLE, se não o avião cai.
    {
        db<Thread>(WRN) << "Removing thread from critical zone priority" << endl;

        _locked = false;
        update();
    }
}

// Since the definition of FCFS above is only known to this unit, forcing its instantiation here so it gets emitted in scheduler.o for subsequent linking with other units is necessary.
template FCFS::FCFS<>(int p);

__END_SYS
