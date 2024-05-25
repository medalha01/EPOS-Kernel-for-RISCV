#ifndef __synchronizer_h
#define __synchronizer_h

#include <architecture.h>
#include <utility/handler.h>
#include <process.h>

__BEGIN_SYS

class Synchronizer_Common
{
protected:
    typedef Thread::Queue Queue;
    typedef EPOS::S::U::List_Elements::Doubly_Linked<EPOS::S::SyncObject> SyncElement;
    typedef List<Synchronizer_Common, List_Elements::Doubly_Linked<Synchronizer_Common>> SynchronizerList;
    typedef EPOS::S::U::List_Elements::Doubly_Linked<EPOS::S::Synchronizer_Common> SemaphoreLink;
    typedef List<SyncObject, List_Elements::Doubly_Linked<SyncObject>> SyncInteractionList;

protected:
    Synchronizer_Common() {}
    ~Synchronizer_Common()
    {
        Thread::lock();

        // Clean up granted queue
        while (!_granted.empty())
        {
            Queue::Element *e = _granted.remove();
            if (e)
                delete e;
        }

        if (!_waiting.empty())
        {
            db<Synchronizer>(WRN) << "~Synchronizer(this=" << this << ") called with active blocked clients!" << endl;
        }

        // Wake up all waiting threads
        wakeup_all();
        Thread::unlock();
    }

    // Atomic operations
    bool tsl(volatile bool &lock) { return CPU::tsl(lock); }
    long finc(volatile long &number) { return CPU::finc(number); }
    long fdec(volatile long &number) { return CPU::fdec(number); }

    // Thread operations
    void lock_for_acquiring()
    {
        Thread::lock();
        // Thread::prioritize(&_granted);
    }
    void _lock()
    {
        Thread::lock();
    }
    void _unlock()
    {
        Thread::unlock();
    }
    void unlock_for_acquiring()
    {
        Queue::Element *e = new (SYSTEM) Queue::Element(Thread::running());
        if (e)
            _granted.insert(e);
        Thread::unlock();
    }

    SyncObject *getMostUrgentInWaiting()
    {
        SyncElement *head_waiter = resource_waiting_list.head();
        if (head_waiter == nullptr)
        {
            return nullptr; // Handle the case where the waiting list is empty
        }

        SyncObject *most_urgent = head_waiter->object();
        SyncElement *current = head_waiter->next();

        while (current != nullptr)
        {
            SyncObject *current_object = current->object();
            if (current_object->getPriority() < most_urgent->getPriority())
            {
                most_urgent = current_object;
            }
            current = current->next();
        }
        return most_urgent;
    }

    void activateCeiling(int priority = Thread::CEILING)
    {
        // Check if the highest priority is set to the ceiling value and the provided priority is lower than the highest priority
        if ((highest_priority) == Thread::CEILING || (priority < highest_priority))
        {
            // Update the highest priority to the new lower priority
            highest_priority = priority;

            // Get the first element in the resource waiting list
            SyncElement *current = resource_holder_list.head();

            // Iterate through the resource waiting list
            while (current != nullptr)
            {
                SyncObject *current_object = current->object();
                if (current_object && current_object->threadPointer)
                {
                    current_object->threadPointer->raise_priority(highest_priority);
                }
                // Move to the next element in the resource waiting list
                current = current->next();
            }
        }
        ceilingIsActive = true;
    }
    void shiftProtocol(Thread *exec_thread)
    {
        if (exec_thread && exec_thread->criticalZonesCount < 1)
        {
            exec_thread->reset_protocol();
        }
        else if (exec_thread)
        {
            exec_thread->get_next_priority();
        }
    }

    void checkForProtocol(Thread *exec_thread)
    {
        if (waitingThreadsCount < 1)
        {
            deactivateCeiling();
        }
    }

    void deactivateCeiling()
    {
        highest_priority = Thread::IDLE;

        SyncElement *current = resource_holder_list.head();

        // Iterate through the resource holder list
        while (current != nullptr)
        {
            SyncObject *current_object = current->object();
            if (current_object && current_object->threadPointer)
            {
                current_object->threadPointer->get_next_priority();
                // resetThreadPriority(current_object->threadPointer); // TODO
            }
            // Move to the next element in the resource waiting list
            current = current->next();
        }
        ceilingIsActive = false;
    }
    /*
    void resetThreadPriority(Thread *exec_thread)
    {
        if (!exec_thread)
            return;

        SyncObject *syncWatcher = exec_thread->_syncHolder;
        if (!syncWatcher)
        {
            exec_thread->restore_priority();
            return;
        }

        SemaphoreLink *syncLink = syncWatcher->synchronizerList.head();
        int starter = Thread::IDLE;

        if (syncLink == nullptr)
        {
            exec_thread->restore_priority();
            return;
        }
        while (syncLink != nullptr)
        {
            Synchronizer_Common *current = syncLink->object();
            if (current && current->ceilingIsActive && current->highest_priority < starter)
            {
                starter = current->highest_priority;
            }
            syncLink = syncLink->next();
        }

        if (starter > exec_thread->_natural_priority)
        {
            exec_thread->restore_priority(); // TODO RESET THREAD UNPRIORITIZE
            return;
        }

        exec_thread->raise_priority(starter);
    }*/

    void lock_for_releasing()
    {
        Thread::lock();
        Queue::Element *e = _granted.remove();
        if (e)
            delete e;
        Thread::deprioritize(&_granted);
        Thread::deprioritize(&_waiting);
    }

    void unlock_for_releasing()
    {
        Thread::unlock();
    }

    void insertSyncObject(SyncObject *resource, SyncInteractionList *list)
    {
        if (resource && list)
        {
            SyncElement *e = new (SYSTEM) SyncElement(resource);
            if (e)
                list->insert(e);
        }
    }

    void removeSyncObject(SyncObject *resource, SyncInteractionList *list)
    {
        if (resource && list)
        {
            SyncElement *removedObject = list->remove(resource);

            if (resource->synchronizerList.empty())
            {
                if (resource->isHolding)
                {
                    if (resource->threadPointer)
                        resource->threadPointer->_syncHolder = nullptr;
                }
                else
                {
                    if (resource->threadPointer)
                        resource->threadPointer->_syncWaiter = nullptr;
                }
                delete resource;
            }

            if (removedObject)
                delete removedObject;
        }
    }

    bool isThreadPresent(Thread *execThread, SyncInteractionList *targetList)
    {
        if (!execThread || !targetList)
            return false;

        SyncElement *syncObject = targetList->head();
        while (syncObject != nullptr)
        {
            if (execThread == syncObject->object()->threadPointer)
            {
                return true;
            }
            syncObject = syncObject->next();
        }
        return false;
    }

    SyncObject *getSyncObject(Thread *execThread, bool isHolder)
    {
        if (!execThread)
            return nullptr;

        SyncObject *resource = nullptr;

        if (isHolder)
        {
            if (execThread->_syncHolder == nullptr)
            {
                resource = new (SYSTEM) SyncObject(execThread, isHolder);
                execThread->_syncHolder = resource;
            }
            else
            {
                resource = execThread->_syncHolder;
            }
        }
        else
        {
            if (execThread->_syncWaiter == nullptr)
            {
                resource = new (SYSTEM) SyncObject(execThread, isHolder);
                execThread->_syncWaiter = resource;
            }
            else
            {
                resource = execThread->_syncWaiter;
            }
        }
        return resource;
    }

    void sleep() { Thread::sleep(&_waiting); }
    void wakeup() { Thread::wakeup(&_waiting); }
    void wakeup_all() { Thread::wakeup_all(&_waiting); }

public:
    int getMostUrgentPriority()
    {
        SyncObject *urgent = getMostUrgentInWaiting();
        return Traits<Synchronizer>::INHERITANCE ? (urgent ? urgent->getPriority() : Thread::IDLE) : Thread::CEILING;
    }

    bool areThreadsWaiting()
    {
        return !(_waiting.empty());
    }

protected:
    Queue _waiting;
    Queue _granted;
    bool ceilingIsActive = false;
    int highest_priority = Thread::IDLE;
    SyncInteractionList resource_holder_list;
    SyncInteractionList resource_waiting_list;
    // int amountOfZones = 0; // TODO: Implement an algorithm to optimize this.
    int waitingThreadsCount = 0;
};

// Mutex class
class Mutex : protected Synchronizer_Common
{
public:
    Mutex();
    ~Mutex();

    void lock();
    void unlock();

private:
    volatile bool _locked;
};

// Semaphore class
class Semaphore : protected Synchronizer_Common
{
public:
    Semaphore(long v = 1);
    ~Semaphore();

    void p();
    void v();

private:
    volatile long _value;
};

// This is actually no Condition Variable
// check http://www.cs.duke.edu/courses/spring01/cps110/slides/sem/sld002.htm
class Condition : protected Synchronizer_Common
{
public:
    Condition();
    ~Condition();

    void wait();
    void signal();
    void broadcast();
};

// An event handler that triggers a mutex (see handler.h)
class Mutex_Handler : public Handler
{
public:
    Mutex_Handler(Mutex *h) : _handler(h) {}
    ~Mutex_Handler() {}

    void operator()()
    {
        if (_handler)
            _handler->unlock();
    }

private:
    Mutex *_handler;
};

// An event handler that triggers a semaphore (see handler.h)
class Semaphore_Handler : public Handler
{
public:
    Semaphore_Handler(Semaphore *h) : _handler(h) {}
    ~Semaphore_Handler() {}

    void operator()()
    {
        if (_handler)
            _handler->v();
    }

private:
    Semaphore *_handler;
};

// An event handler that triggers a condition variable (see handler.h)
class Condition_Handler : public Handler
{
public:
    Condition_Handler(Condition *h) : _handler(h) {}
    ~Condition_Handler() {}

    void operator()()
    {
        if (_handler)
            _handler->signal();
    }

private:
    Condition *_handler;
};

// SyncObject class
class SyncObject
{
    friend Synchronizer_Common;
    friend Thread;

public:
    typedef Thread::Queue ThreadQueue;
    typedef EPOS::S::U::List_Elements::Doubly_Linked<EPOS::S::SyncObject> SyncElement;
    typedef List<Synchronizer_Common, List_Elements::Doubly_Linked<Synchronizer_Common>> SynchronizerList;
    typedef EPOS::S::U::List_Elements::Doubly_Linked<EPOS::S::Synchronizer_Common> SemaphoreLink;

    SyncObject(Thread *thread, bool holding)
        : threadPointer(thread), isHolding(holding)
    {
        selfReferencePointer = new (SYSTEM) SyncElement(this);
        if (selfReferencePointer)
        {
            if (holding)
                threadPointer->_syncHolder = this;
            else
                threadPointer->_syncWaiter = this;
        }
    }

    ~SyncObject()
    {
        // TODO esvaziar listas e destruir objetos vazios

        if (selfReferencePointer)
        {
            delete selfReferencePointer;
        }
    }

    void addSynchronizer(Synchronizer_Common *syncObj)
    {
        if (syncObj)
        {
            SemaphoreLink *syncronizerLink = new (SYSTEM) SemaphoreLink(syncObj);
            if (syncronizerLink)
                synchronizerList.insert(syncronizerLink);
        }
    }
    void removeSynchronizer(Synchronizer_Common *syncObj)
    {
        if (syncObj)
        {
            SemaphoreLink *syncronizerLink = synchronizerList.remove(syncObj);
            if (syncronizerLink)
            {
                delete syncronizerLink;
            }
        }
    }

    int getPriority()
    {
        return threadPointer ? threadPointer->criterion()._priority : Thread::IDLE;
    }

    Thread *threadPointer = nullptr;
    SyncElement *selfReferencePointer = nullptr;
    bool isHolding = false;
    SynchronizerList synchronizerList;
};

__END_SYS

#endif // __synchronizer_h
