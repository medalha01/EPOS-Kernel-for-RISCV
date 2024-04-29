// EPOS Synchronizer Components

#ifndef __synchronizer_h
#define __synchronizer_h

#include <architecture.h>
#include <utility/handler.h>
#include <process.h>
#include <utility/list.h>

__BEGIN_SYS

class Synchronizer_Common
{
protected:
    typedef Thread::Queue Queue;

protected:
    Synchronizer_Common() {}
    ~Synchronizer_Common()
    {
        begin_atomic();
        wakeup_all();
        end_atomic();
    }

    // Atomic operations
    bool tsl(volatile bool &lock) { return CPU::tsl(lock); }
    long finc(volatile long &number) { return CPU::finc(number); }
    long fdec(volatile long &number) { return CPU::fdec(number); }

    // Thread operations
    void begin_atomic() { Thread::lock(); }
    void end_atomic() { Thread::unlock(); }

    void sleep() { Thread::sleep(&_queue); }
    void wakeup() { Thread::wakeup(&_queue); }
    void wakeup_all() { Thread::wakeup_all(&_queue); }

protected:
    Queue _queue;
};

class Mutex : protected Synchronizer_Common
{
public:
    Mutex(bool = false, bool inheritance = false);
    ~Mutex();

    void lock();
    void unlock();

private:
    volatile bool _locked;
    bool _hasCeiling;
    bool _hasHeritance;
    bool incrementFlag = true;
    List<Thread, List_Elements::Doubly_Linked<Thread>> resource_holder_list;
    List<Thread, List_Elements::Doubly_Linked<Thread>> waiting_threads;
};

class Semaphore : protected Synchronizer_Common
{
public:
    Semaphore(long v = 1, bool = false, bool _inheritance = false);
    ~Semaphore();

    void p();
    void v();

private:
    void _addResource(Thread *, List<Thread, List_Elements::Doubly_Linked<Thread>> *);
    void _removeResource(Thread *, List<Thread, List_Elements::Doubly_Linked<Thread>> *);
    Thread *_getHighestPriorityThread(List<Thread, List_Elements::Doubly_Linked<Thread>> *, bool);

private:
    volatile long _value;
    bool _hasCeiling;
    bool _inheritance;
    Thread **resource_holders;
    bool incrementFlag = true;

    List<Thread, List_Elements::Doubly_Linked<Thread>> resource_holder_list;
    List<Thread, List_Elements::Doubly_Linked<Thread>> waiting_threads;

    Thread *
        _lock_holder;
    Thread *_most_urgent_in_await;
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

__END_SYS

#endif
