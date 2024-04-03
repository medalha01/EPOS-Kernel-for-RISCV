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

LLF::LLF(const Microsecond &_deadline, const Microsecond &_period, const Microsecond &_capacity, unsigned int) : Real_Time_Scheduler_Common(Alarm::ticks(_deadline), Alarm::ticks(_deadline), _period, _capacity) {}

void LLF::set_start()
{
    _init_time = Alarm::elapsed();
}
void LLF::update()
{
    if ((_priority >= PERIODIC) && (_priority < APERIODIC))
    {

        if (_remaining_time == 0)
        {
            _remaining_time = _capacity;
            _priority = Alarm::elapsed() + _deadline - _remaining_time;
        }
        else
        {
            _remaining_time = _remaining_time - Alarm::elapsed() - _init_time;
            _priority = Alarm::elapsed() + _deadline - _remaining_time;
        }
        db<LLF>(WRN) << "\nLLF::update() => " << _priority << endl;
        db<LLF>(WRN) << "Remaining_Time => " << _remaining_time << endl;
        db<LLF>(WRN) << "Init_Time => " << _init_time << endl;
        db<LLF>(WRN) << "Deadline => " << _deadline << endl;
        db<LLF>(WRN) << "Capacity => " << _capacity << endl;
        db<LLF>(WRN) << "Elapsed => " << Alarm::elapsed() << endl;
    }
}
// Since the definition of FCFS above is only known to this unit, forcing its instantiation here so it gets emitted in scheduler.o for subsequent linking with other units is necessary.
template FCFS::FCFS<>(int p);

__END_SYS
