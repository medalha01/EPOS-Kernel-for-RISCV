#ifndef __synchronizer_h
#define __synchronizer_h

#include <architecture.h>
#include <utility/handler.h>
#include <process.h>

__BEGIN_SYS

class Synchronizer_Common;

class SyncIterator
{
    friend class Synchronizer_Common;
    friend class SyncObject;
    friend class Thread;

public:
    SyncIterator(Thread *thread, Synchronizer_Common *synchronizer)
    {
        tp = thread;
        syncObject = synchronizer;
    }
    Thread *tp;
    Synchronizer_Common *syncObject;
    int counter = 0;
};

class Synchronizer_Common
{
    friend class Thread;

protected:
    typedef Thread::Queue Sync_Queue;
    typedef Thread::Queue::Element T_Sync_Element;

public:
    typedef List<Synchronizer_Common, List_Elements::Doubly_Linked<Synchronizer_Common>> SynchronizerList;
    typedef EPOS::S::U::List_Elements::Doubly_Linked<EPOS::S::Synchronizer_Common> SemaphoreLink;

protected:
    Synchronizer_Common() {}
    ~Synchronizer_Common()
    {
        Thread::lock();

        // Clean up granted queue

        if (!_waiting.empty())
        {
            db<Synchronizer>(WRN) << "~Synchronizer(this=" << this << ") called with active blocked clients!" << endl;
        }

        while (!resource_holder_list.empty())
        {
            T_Sync_Element *e = resource_holder_list.remove();
            if (e)
            {
                e->object()->removeSynchronizer(this);
                delete e->object();
            }
        }

        while (!resource_waiting_list.empty())
        {
            T_Sync_Element *e = resource_holder_list.remove();
            if (e)
            {
                delete e->object();
            }
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

    Thread *getMostUrgentInWaiting()
    {
        T_Sync_Element *current = resource_waiting_list.head();
        if (current == nullptr)
        {
            return nullptr; // Handle the case where the waiting list is empty
        }

        Thread *most_urgent = current->object();

        while (current != nullptr)
        {
            Thread *current_object = current->object();
            if (current_object->priority() < most_urgent->priority())
            {
                most_urgent = current_object;
            }
            if (most_urgent->priority() == Thread::CEILING)
            {
                highest_priority = most_urgent->priority();
                return most_urgent;
            }
            current = current->next();
        }
        highest_priority = most_urgent->priority();
        return most_urgent;
    }

    void activateCeiling(int priority = Thread::CEILING)
    {
        ceilingIsActive = true;

        // Check if the highest priority is set to the ceiling value and the provided priority is lower than the highest priority
        if ((highest_priority) == Thread::CEILING || (priority < highest_priority))
        {
            // Update the highest priority to the new lower priority
            highest_priority = priority;

            // Get the first element in the resource waiting list
            T_Sync_Element *current = resource_holder_list.head();

            // Iterate through the resource waiting list
            while (current != nullptr)
            {
                Thread *current_object = current->object();

                if (current_object)
                {
                    current_object->raise_priority(highest_priority);
                }

                // Move to the next element in the resource waiting list
                current = current->next();
            }
        }
    }

    void shiftProtocol(Thread *exec_thread)
    {
        if (exec_thread && exec_thread->criticalZonesCount < 1)
        {
            exec_thread->reset_protocol();
        }
        else if (exec_thread)
        {
            // TODO REAVALIAR MAS PARECE BOM PLANO
            exec_thread->reset_protocol();
            int most_urgent_from_syncs = get_new_priority(exec_thread);
            if (most_urgent_from_syncs < exec_thread->_natural_priority)
            {
                exec_thread->raise_priority(most_urgent_from_syncs);
            }
        }
    }

    int get_new_priority(Thread *exec_thread)
    {
        if (exec_thread->synchronizerList.empty())
        {
            return Thread::idle();
        }
        SemaphoreLink *helper = exec_thread->synchronizerList.head();
        Synchronizer_Common *current = helper->object();

        int highest_t_priority = current->getMostUrgentPriority();

        while (helper)
        {
            current = helper->object();
            if (!current->areThreadsWaiting())
            {
                helper = helper->next();
                continue;
            }
            int current_priority = current->getMostUrgentPriority();

            if (current_priority < highest_t_priority)
            {
                highest_t_priority = current_priority;
            }
            if (highest_t_priority == Thread::CEILING && current->ceilingIsActive)
            {
                return Thread::CEILING;
            }
            helper = helper->next();
        }
        return highest_t_priority;
    }

    void checkForThreadProtocol(Thread *exec_thread)
    {
        if (areThreadsWaiting())
        {
            exec_thread->raise_priority(get_new_priority(exec_thread));
        }
    }

    void checkForProtocol(Thread *exec_thread)
    {
        if (!areThreadsWaiting())
        {
            deactivateCeiling();
        } // TODO LEAVING THREAD MUST ENTER PROTOCOL
        else
        {
            shiftProtocol(exec_thread);
        }
    }

    void recalculatePriorities()
    {
        if (!areThreadsWaiting())
        {
            deactivateCeiling();
        }

        T_Sync_Element *current = resource_holder_list.head();
        while (current != nullptr)
        {
            Thread *current_object = current->object();
            if (current_object)
            {
                int new_prio = get_new_priority(current_object);
                current_object->reset_protocol();
                current_object->raise_priority(new_prio);
            }
            current = current->next();
        }
    }

    void deactivateCeiling()
    {
        highest_priority = Thread::IDLE;
        ceilingIsActive = false;
    }

    void insertSyncObject(Thread *resource, Sync_Queue *list)
    {
        if (resource && list)
        {
            T_Sync_Element *e = new (SYSTEM) T_Sync_Element(resource);
            if (e)
                list->insert(e);
        }
    }

    void removeSyncObject(Thread *resource, Sync_Queue *list)
    {
        if (resource && list)
        {
            T_Sync_Element *removedObject = list->remove(resource);

            if (removedObject)
                delete removedObject;
        }
    }
    bool isThreadPresent(Thread *execThread, Sync_Queue *targetList)
    {
        if (!execThread || !targetList)
            return false;

        T_Sync_Element *temp_thread_link = targetList->head();
        while (temp_thread_link != nullptr)
        {
            if (execThread == temp_thread_link->object())
            {
                return true;
            }
            temp_thread_link = temp_thread_link->next();
        }
        return false;
    }

    void sleep() { Thread::sleep(&_waiting); }
    void wakeup() { Thread::wakeup(&_waiting); }
    void wakeup_all() { Thread::wakeup_all(&_waiting); }

public:
    int getMostUrgentPriority()
    {
        Thread *urgent = getMostUrgentInWaiting();
        if (urgent)
        {
            highest_priority = urgent->priority();
        }
        else
        {
            highest_priority = Thread::IDLE;
        }
        return Traits<Synchronizer>::INHERITANCE ? (highest_priority) : Thread::CEILING;
    }

    bool areThreadsWaiting()
    {
        return !(resource_waiting_list.empty());
    }

protected:
    Sync_Queue _waiting;

    bool ceilingIsActive = false;

    int highest_priority = Thread::IDLE;

    Sync_Queue resource_holder_list;
    Sync_Queue resource_waiting_list;

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

__END_SYS

#endif // __synchronizer_h
