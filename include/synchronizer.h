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
    typedef EPOS::S::U::List_Elements::Doubly_Linked<EPOS::S::Sync_Object> Queue_Element;
    typedef List<Synchronizer_Common, List_Elements::Doubly_Linked<Synchronizer_Common>> Synchronizer_List;

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
        Thread::prioritize(&_granted);
    }

    void unlock_for_acquiring()
    {
        _granted.insert(new (SYSTEM) Queue::Element(Thread::running()));
        Thread::unlock();
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

    void sleep() { Thread::sleep(&_waiting); }
    void wakeup() { Thread::wakeup(&_waiting); }
    void wakeup_all() { Thread::wakeup_all(&_waiting); }

protected:
    Queue _waiting;
    Queue _granted;
    List<Sync_Object, List_Elements::Doubly_Linked<Sync_Object>> resource_holder_list;
    List<Sync_Object, List_Elements::Doubly_Linked<Sync_Object>> resource_waiting_list;
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

class Sync_Object
{
    friend Synchronizer_Common;
    friend Thread;

public:
    typedef Thread::Queue Queue;
    typedef EPOS::S::U::List_Elements::Doubly_Linked<EPOS::S::Sync_Object> Queue_Element;

    Sync_Object(Thread *thread_pointer, Semaphore *semaphore_pointer, bool isHolding)
    {
        tp = thread_pointer;
        smpp = semaphore_pointer;
        reference_pointer = new (SYSTEM) Queue_Element(this);
        isHolding = isHolding;
    }

    Sync_Object(Thread *thread_pointer, Mutex *mutex_pointer, bool isHolding)
    {
        tp = thread_pointer;
        mup = mutex_pointer;
        reference_pointer = new (SYSTEM) Queue_Element(this);
        isHolding = isHolding;
    }
    ~Sync_Object()
    {
        if (reference_pointer)
        {
            delete reference_pointer;
        }
    }

    void add_synchronizer(Queue::Element *syncObj) { synchronizer_list.insert(syncObj); }
    void remove_synchronizer(Queue::Element *syncObj) { synchronizer_list.remove(syncObj); }

    Thread *tp = nullptr;
    Semaphore *smpp = nullptr;
    Mutex *mup = nullptr;
    Queue_Element *reference_pointer = nullptr;
    bool isHolding = false;
    Queue synchronizer_list;
};

__END_SYS

#endif
