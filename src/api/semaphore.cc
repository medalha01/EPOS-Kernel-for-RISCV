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
    _lock();
    if (Traits<Synchronizer>::CEILING_PROTOCOL)
    {
        Thread *exec_thread = Thread::self();

        if (fdec(_value) < 1)
        {
            // Get the SyncObject for the current thread, without creating a new one if it does exist
            SyncObject *syncWatch = getSyncObject(exec_thread, false);
            // Add this mutex to the list of synchronizers being watched by the SyncObject
            syncWatch->addSynchronizer(this);
            // Insert the SyncObject into the resource waiting list
            insertSyncObject(syncWatch, &resource_waiting_list);
            waitingThreadsCount++;
            activateCeiling(getMostUrgentPriority());
            // Put the current thread to sleep until the mutex becomes available
            sleep();

            waitingThreadsCount--;
            // After waking up, we clean up the SyncObject
            // SyncObject *syncWatch = getSyncObject(exec_thread, false);
            syncWatch->removeSynchronizer(this);
            removeSyncObject(syncWatch, &resource_waiting_list);

            // TODO NOW WE HOLD THE RESOURCE
        }
        exec_thread->enter_zone();

        // Get the SyncObject for the current thread, without creating a new one if it does exist
        SyncObject *syncWatchHolder = getSyncObject(exec_thread, true);
        // Add this mutex to the list of synchronizers being watched by the SyncObject
        syncWatchHolder->addSynchronizer(this);
        // Insert the SyncObject into the resource holder list
        insertSyncObject(syncWatchHolder, &resource_holder_list);
        // check for new priority is the thread is leaving the waiting zone, and to see if they mustlock
        checkForProtocol(exec_thread);
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
    if (Traits<Synchronizer>::CEILING_PROTOCOL)
    {
        Thread *exec_thread = Thread::self();

        if (finc(_value) < 0)
            wakeup();

        exec_thread->leave_zone();

        SyncObject *syncWatch = getSyncObject(exec_thread, true);
        syncWatch->removeSynchronizer(this);
        removeSyncObject(syncWatch, &resource_holder_list);

        shiftProtocol(exec_thread);
    }
    else
    {
        wakeup();
    }
    _unlock();
}

__END_SYS
