// EPOS Semaphore Implementation

#include <synchronizer.h>

__BEGIN_SYS

Semaphore::Semaphore(long value, bool useCeiling) : _value(value), _hasCeiling(useCeiling)
{
    db<Synchronizer>(TRC) << "Semaphore(value=" << _value << ") => " << this << endl;
}

Semaphore::~Semaphore()
{
    db<Synchronizer>(TRC) << "~Semaphore(this=" << this << ")" << endl;
}

void Semaphore::p()
{
    Thread *running_thread = Thread::running();

    db<Synchronizer>(TRC)
        << "Semaphore::p(this=" << this << ",value=" << _value << ")" << endl;
    begin_atomic();
    if (fdec(_value) < 1)
    {
        if (_hasCeiling)
            Thread::start_periodic_critical(_lock_holder);
        sleep();
    }
    if (!_lock_holder)
        _lock_holder = running_thread;
    end_atomic();
}

void Semaphore::v()
{
    Thread *running_thread = Thread::running();

    db<Synchronizer>(TRC)
        << "Semaphore::v(this=" << this << ",value=" << _value << ")" << endl;

    begin_atomic();
    if (finc(_value) < 0)
        wakeup();
    if (running_thread == _lock_holder)
        _lock_holder = nullptr;
    if (_hasCeiling)
        Thread::end_periodic_critical(running_thread);
    end_atomic();
}

__END_SYS
