// EPOS CPU Scheduler Component Implementation

#include <process.h>
#include <time.h>

__BEGIN_SYS

inline RT_Common::Tick RT_Common::elapsed() { return Alarm::elapsed(); }
volatile unsigned int PLLF::_next_queue;

RT_Common::Tick RT_Common::ticks(Microsecond time)
{
    return Timer_Common::ticks(time, Alarm::timer()->frequency());
}

Microsecond RT_Common::time(Tick ticks)
{
    return Timer_Common::time(ticks, Alarm::timer()->frequency());
}

void RT_Common::handle(Event event)
{
    db<Thread>(TRC) << "RT::handle(this=" << this << ",e=";
    if (event & CREATE)
    {
        db<Thread>(TRC) << "CREATE";

        _statistics.thread_creation = elapsed();
        _statistics.job_released = false;
    }
    if (event & FINISH)
    {
        db<Thread>(TRC) << "FINISH";

        _statistics.thread_destruction = elapsed();
        _statistics.thread_deadline_lack = _deadline - elapsed();
    }
    if (event & ENTER)
    {
        db<Thread>(TRC) << "ENTER";
        _statistics.thread_total_executions++;
        _statistics.thread_last_dispatch = elapsed();
        _statistics.thread_wait_time = elapsed() - _statistics.thread_last_preemption;
    }
    if (event & LEAVE)
    {
        Tick cpu_time = elapsed() - _statistics.thread_last_dispatch;

        db<Thread>(TRC) << "LEAVE";

        _statistics.thread_last_preemption = elapsed();
        _statistics.thread_execution_time += cpu_time;

//        if(_statistics.job_released) {
            _statistics.job_utilization += cpu_time;
//        }
        if (cpu_time > _statistics.thread_max_execution_time) {
            _statistics.thread_max_execution_time = cpu_time;
        }
        if (cpu_time < _statistics.thread_min_execution_time || _statistics.thread_min_execution_time == 0) {
            _statistics.thread_min_execution_time = cpu_time;
        }
        if (elapsed() > _deadline) {
            _statistics.thread_missed_deadline++;
        }
    }
    if (periodic() && (event & JOB_RELEASE))
    {
        db<Thread>(TRC) << "RELEASE";

        _statistics.job_released = true;
        _statistics.job_release = elapsed();
        _statistics.job_start = 0;
        _statistics.job_utilization = 0;
        _statistics.jobs_released++;
    }
    if (periodic() && (event & JOB_FINISH))
    {
        db<Thread>(TRC) << "WAIT";

        Tick job_time = elapsed() - _statistics.job_release;

        _statistics.job_total_execution_time += job_time;
        _statistics.job_released = false;
        _statistics.job_finish = elapsed();
        _statistics.jobs_finished++;

        _statistics.job_avg_execution_time = _statistics.job_total_execution_time / _statistics.jobs_finished;

        if (_statistics.job_max_execution_time < job_time) {
            _statistics.job_max_execution_time = job_time;
        }
        if (_statistics.job_min_execution_time > job_time || _statistics.job_min_execution_time == 0) {
            _statistics.job_min_execution_time = job_time;
        }
    }
    if (event & COLLECT)
    {
        db<Thread>(TRC) << "|COLLECT";
    }
    if (periodic() && (event & CHARGE))
    {
        db<Thread>(TRC) << "|CHARGE";
    }
    if (periodic() && (event & AWARD))
    {
        db<Thread>(TRC) << "|AWARD";
    }
    if (periodic() && (event & UPDATE))
    {
        db<Thread>(TRC) << "|UPDATE";
    }
    db<Thread>(TRC) << ") => {i=" << _priority << ",p=" << _period << ",d=" << _deadline << ",c=" << _capacity << "}" << endl;
}

template <typename... Tn>
FCFS::FCFS(int p, Tn &...an) : Priority((p == IDLE) ? IDLE : RT_Common::elapsed()) {}

EDF::EDF(Microsecond p, Microsecond d, Microsecond c) : RT_Common(int(elapsed() + ticks(d)), p, d, c) {}

void EDF::handle(Event event)
{
    RT_Common::handle(event);

    // Update the priority of the thread at job releases, before _alarm->v(), so
    // it enters the queue in the right order (called from
    // Periodic_Thread::Xxx_Handler)
    if (event & JOB_RELEASE)
        _priority = elapsed() + _deadline;
}

LLF::LLF(Microsecond p, Microsecond d, Microsecond c, unsigned int): 
	RT_Common(int(elapsed() + ticks((d ? d : p) - c)), p, d, c)
{ }



void LLF::handle(Event event)
{
    if ((event & UPDATE) | (event & JOB_RELEASE) | (event & JOB_FINISH))
    {
        _priority = elapsed() + _deadline - _capacity + _statistics.job_utilization;
    }
    RT_Common::handle(event);
}

template FCFS::FCFS<>(int p);

__END_SYS
