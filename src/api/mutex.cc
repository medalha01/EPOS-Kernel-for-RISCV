// EPOS Mutex Implementation

#include <synchronizer.h>

__BEGIN_SYS

Mutex::Mutex(bool useCeiling, bool useHeritance) : _locked(false), _hasCeiling(useCeiling), _hasHeritance(useHeritance)
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
    Thread *t = Thread::running();
    int temp_priority = Thread::CEILING;
    if (_hasHeritance)
        temp_priority = t->criterion()._priority;

    begin_atomic();
    if (tsl(_locked))
        sleep();
    if (_hasCeiling)
        Thread::start_periodic_critical(t, incrementFlag, temp_priority);
    incrementFlag = false;

    end_atomic();
}

void Mutex::unlock()
{
    db<Synchronizer>(TRC) << "Mutex::unlock(this=" << this << ")" << endl;
    Thread *t = Thread::running();
    begin_atomic();
    if (_queue.empty())
        _locked = false;
    else
        wakeup();
    if (_hasCeiling)
        Thread::end_periodic_critical(t, incrementFlag);
    incrementFlag = true;

    end_atomic();
}

__END_SYS
