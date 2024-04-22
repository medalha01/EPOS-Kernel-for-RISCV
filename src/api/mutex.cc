// EPOS Mutex Implementation

#include <synchronizer.h>

__BEGIN_SYS

Mutex::Mutex(bool useCeiling) : _locked(false), _hasCeiling(useCeiling)
{
    db<Synchronizer>(TRC) << "Mutex() => " << this << endl;
}

Mutex::~Mutex()
{
    db<Synchronizer>(TRC) << "~Mutex(this=" << this << ")" << endl;
}

void Mutex::lock()
{
    db<Synchronizer>(TRC) << "Mutex::lock(this=" << this << ")" << endl;

    begin_atomic();
    if (tsl(_locked))
        sleep();
    if (_hasCeiling)
        Thread::start_critical();

    end_atomic();
}

void Mutex::unlock()
{
    db<Synchronizer>(TRC) << "Mutex::unlock(this=" << this << ")" << endl;

    begin_atomic();
    if (_queue.empty())
        _locked = false;
    else
        wakeup();
    if (_hasCeiling)
        Thread::end_critical();
    end_atomic();
}

__END_SYS
