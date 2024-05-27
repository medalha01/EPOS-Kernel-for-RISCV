// EPOS Mutex Implementation

#include <synchronizer.h>

__BEGIN_SYS

Mutex::Mutex() : _locked(false)
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
    _lock();
    if (Traits<Synchronizer>::CEILING_PROTOCOL)
    {
        Thread *exec_thread = Thread::self();

        if (tsl(_locked))
        {
            // Get the SyncObject for the current thread, without creating a new one if it does exist
            waitingThreadsCount++;
            insertSyncObject(exec_thread, &resource_waiting_list);
            activateCeiling(getMostUrgentPriority());

            // Put the current thread to sleep until the mutex becomes available
            sleep();

            waitingThreadsCount--;
            removeSyncObject(exec_thread, &resource_waiting_list);
            recalculatePriorities();

            // TODO NOW WE HOLD THE RESOURCE
        }

        exec_thread->enter_zone();
        exec_thread->addSynchronizer(this);
        // Insert the SyncObject into the resource holder list
        insertSyncObject(exec_thread, &resource_holder_list);
        checkForThreadProtocol(exec_thread);
    }
    else
    {
        if (tsl(_locked))
        {
            sleep();
        }
    }
    _unlock();
}

void Mutex::unlock()
{
    db<Synchronizer>(TRC) << "Mutex::unlock(this=" << this << ")" << endl;

    _lock();
    if (Traits<Synchronizer>::CEILING_PROTOCOL)
    {
        Thread *exec_thread = Thread::self();

        if (_waiting.empty())
            _locked = false;
        else
            wakeup();

        exec_thread->leave_zone();
        exec_thread->removeSynchronizer(this);
        removeSyncObject(exec_thread, &resource_holder_list);
        shiftProtocol(exec_thread);
    }
    else
    {
        if (_waiting.empty())
            _locked = false;
        else
            wakeup();
    }
    _unlock();
}

__END_SYS
