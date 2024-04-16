// EPOS Semaphore Implementation

#include <synchronizer.h>

__BEGIN_SYS

Semaphore::Semaphore(long v) : _value(v)
{
    db<Synchronizer>(TRC) << "Semaphore(value=" << _value << ") => " << this << endl;
}

Semaphore::~Semaphore()
{
    db<Synchronizer>(TRC) << "~Semaphore(this=" << this << ")" << endl;
}

void Semaphore::p()
{
    db<Synchronizer>(TRC) << "Semaphore::p(this=" << this << ",value=" << _value << ")" << endl;

    begin_atomic();
    if (fdec(_value) < 1)
    {
        Thread::periodic_critical(_lock_holder);
        sleep();
    }
    end_atomic();

    if (_value == 0)
    {
        _lock_holder = Thread::running();
    }
}

void Semaphore::v()
{
    db<Synchronizer>(TRC) << "Semaphore::v(this=" << this << ",value=" << _value << ")" << endl;
    begin_atomic();
    if (finc(_value) < 0)
        wakeup();
    Thread::end_critical();

    end_atomic();
}

__END_SYS
