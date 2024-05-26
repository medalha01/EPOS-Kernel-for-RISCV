// EPOS Semaphore Implementation

#include <synchronizer.h>

__BEGIN_SYS

extern OStream kout;

Semaphore::Semaphore(long v, bool is_producer) : _value(v), producer(is_producer)
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
    _lock();
    if (Traits<Synchronizer>::CEILING_PROTOCOL && !producer)
    {
        Thread *exec_thread = Thread::self();

        if (fdec(_value) < 1)
        {
            waitingThreadsCount++;
            insertSyncObject(exec_thread, &resource_waiting_list);
            activateCeiling(getMostUrgentPriority());
            _print("Activating ceiling protocol\n");

            sleep();

            waitingThreadsCount--;
            removeSyncObject(exec_thread, &resource_waiting_list);
            recalculatePriorities();
        }

        exec_thread->enter_zone();
        exec_thread->addSynchronizer(this);
        // Insert the SyncObject into the resource holder list
        insertSyncObject(exec_thread, &resource_holder_list);
        checkForThreadProtocol(exec_thread);
    }
    else
    {
        if (fdec(_value) < 1)
        {
            sleep();
        }
    }
    _unlock();
}

void Semaphore::v()
{
    db<Synchronizer>(TRC) << "Semaphore::v(this=" << this << ",value=" << _value << ")" << endl;

    _lock();
    if (Traits<Synchronizer>::CEILING_PROTOCOL && !producer)
    {
        Thread *exec_thread = Thread::self();

        if (finc(_value) < 0)
            wakeup();

        exec_thread->leave_zone();
        exec_thread->removeSynchronizer(this);
        removeSyncObject(exec_thread, &resource_holder_list);
        shiftProtocol(exec_thread);
    }
    else
    {
        if (finc(_value) < 0)

            wakeup();
    }
    _unlock();
}

__END_SYS
