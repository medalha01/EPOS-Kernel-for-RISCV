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
        while (tsl(_locked))
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
