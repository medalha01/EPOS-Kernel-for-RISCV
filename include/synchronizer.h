// EPOS Synchronizer Components

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
        _granted.insert(new (SYSTEM) Queue::Element(Thread::running()));
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
        list->insert(new (SYSTEM) SyncElement(resource));
    }

    void removeSyncObject(SyncObject *resource, SyncInteractionList *list)
    {
        SyncElement *removedObject = list->remove(resource);

        if (resource->synchronizerList.empty())
        {
            if (resource->isHolding)
            {
                resource->threadPointer->_syncHolder = nullptr;
            }
            else
            {
                resource->threadPointer->_syncWaiter = nullptr;
            }
            if (resource)
                delete resource;
        }

        if (removedObject)
            delete removedObject;
    }

    bool isThreadPresent(Thread *execThread, SyncInteractionList *targetList)
    {
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
        SyncObject *resource = nullptr;

        if (isHolder)
        {
            if (execThread->_syncHolder == nullptr)
            {
                resource = new SyncObject(execThread, isHolder);
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
                resource = new SyncObject(execThread, isHolder);
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

protected:
    Queue _waiting;
    Queue _granted;
    int highest_priority = Thread::CEILING;
    SyncInteractionList resource_holder_list;
    SyncInteractionList resource_waiting_list;
};

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

    void operator()() { _handler->unlock(); }

private:
    Mutex *_handler;
};

// An event handler that triggers a semaphore (see handler.h)
class Semaphore_Handler : public Handler
{
public:
    Semaphore_Handler(Semaphore *h) : _handler(h) {}
    ~Semaphore_Handler() {}

    void operator()() { _handler->v(); }

private:
    Semaphore *_handler;
};

// An event handler that triggers a condition variable (see handler.h)
class Condition_Handler : public Handler
{
public:
    Condition_Handler(Condition *h) : _handler(h) {}
    ~Condition_Handler() {}

    void operator()() { _handler->signal(); }

private:
    Condition *_handler;
};

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
        if (holding)
            threadPointer->_syncHolder = this;
        else
            threadPointer->_syncWaiter = this;
    }

    ~SyncObject()
    {
        // todo esvaziar listas e destruir objetos vazios
        if (selfReferencePointer)
        {
            delete selfReferencePointer;
        }
    }

    void addSynchronizer(Synchronizer_Common *syncObj)
    {
        SemaphoreLink *syncronizerLink = new (SYSTEM) SemaphoreLink(syncObj);
        synchronizerList.insert(syncronizerLink);
    }
    void removeSynchronizer(Synchronizer_Common *syncObj)
    {
        SemaphoreLink *syncronizerLink = synchronizerList.remove(syncObj);
        if (syncronizerLink)
        {
            delete syncronizerLink;
        }
    }

    int getPriority()
    {
        return threadPointer->criterion()._priority;
    }

    Thread *threadPointer = nullptr;
    SyncElement *selfReferencePointer = nullptr;
    bool isHolding = false;
    SynchronizerList synchronizerList;
};

__END_SYS

#endif
