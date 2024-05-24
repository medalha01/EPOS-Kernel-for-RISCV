// EPOS Thread Component Declarations

#ifndef __process_h
#define __process_h

#include <architecture.h>
#include <machine.h>
#include <utility/queue.h>
#include <utility/handler.h>
#include <scheduler.h>

extern "C"
{
    void __exit();
    void _lock_heap();
    void _unlock_heap();
}

__BEGIN_SYS

class Thread
{
    friend class Init_End;            // context->load()
    friend class Init_System;         // for init() on CPU != 0
    friend class Scheduler<Thread>;   // for link()
    friend class Synchronizer_Common; // for lock() and sleep()
    friend class Alarm;               // for lock()
    friend class System;              // for init()
    friend class IC;                  // for link() for priority ceiling
    friend class SyncObject;
    friend void ::_lock_heap();   // for lock()
    friend void ::_unlock_heap(); // for unlock()

protected:
    static const bool preemptive = Traits<Thread>::Criterion::preemptive;
    static const int priority_inversion_protocol = 1;
    static const unsigned int QUANTUM = Traits<Thread>::QUANTUM;
    static const unsigned int STACK_SIZE = Traits<Application>::STACK_SIZE;

    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::Context Context;

public:
    // Thread State
    enum State
    {
        RUNNING,
        READY,
        SUSPENDED,
        WAITING,
        FINISHING
    };

    // Thread Scheduling Criterion
    typedef Traits<Thread>::Criterion Criterion;
    enum
    {
        CEILING = Criterion::CEILING,
        HIGH = Criterion::HIGH,
        NORMAL = Criterion::NORMAL,
        LOW = Criterion::LOW,
        MAIN = Criterion::MAIN,
        IDLE = Criterion::IDLE
    };

    // Thread Queue
    typedef Ordered_Queue<Thread, Criterion, Scheduler<Thread>::Element> Queue;

    // Thread Configuration
    struct Configuration
    {
        Configuration(State s = READY, Criterion c = NORMAL, unsigned int ss = STACK_SIZE)
            : state(s), criterion(c), stack_size(ss) {}

        State state;
        Criterion criterion;
        unsigned int stack_size;
    };

public:
    template <typename... Tn>
    Thread(int (*entry)(Tn...), Tn... an);
    template <typename... Tn>
    Thread(Configuration conf, int (*entry)(Tn...), Tn... an);
    ~Thread();

    const volatile State &state() const { return _state; }
    Criterion &criterion() { return const_cast<Criterion &>(_link.rank()); }
    volatile Criterion::Statistics &statistics() { return criterion().statistics(); }

    const volatile Criterion &priority() const { return _link.rank(); }
    void priority(Criterion p);

    int join();
    void pass();
    void suspend();
    void resume();

    static Thread *volatile self();
    static void yield();
    static void exit(int status = 0);

protected:
    void constructor_prologue(unsigned int stack_size);
    void constructor_epilogue(Log_Addr entry, unsigned int stack_size);

    Queue::Element *link() { return &_link; }

    static Thread *volatile running() { return _scheduler.chosen(); }

    /*static void lock() { CPU::int_disable(); }
    static void unlock() { CPU::int_enable(); }*/
    static void lock(Spin *lock = &_lock)
    {
        CPU::int_disable();
        if (Traits<Machine>::multi)
        {
            lock->acquire();
        }
    }
    //
    static void unlock(Spin *lock = &_lock)
    {
        if (Traits<Machine>::multi)
        {
            lock->release();
        }
        if (_not_booting)
        {
            CPU::int_enable();
        }
    }
    static volatile bool locked() { return (Traits<Machine>::multi) ? _lock.taken() : CPU::int_disabled(); }

    static void sleep(Queue *queue);
    static void wakeup(Queue *queue);
    static void wakeup_all(Queue *queue);

    void raise_priority(int criterion);
    static void prioritize(Queue *queue);
    static void deprioritize(Queue *queue);

    void enter_zone()
    {
        criticalZonesCount++;
    };

    void leave_zone()
    {
        criticalZonesCount--;
    };

    static void reschedule();
    static void reschedule(unsigned int cpu);
    static void int_rescheduler(IC::Interrupt_Id i);
    static void time_slicer(IC::Interrupt_Id interrupt);

    static void dispatch(Thread *prev, Thread *next, bool charge = true);

    static void for_all_threads(Criterion::Event event)
    {
        for (Queue::Iterator i = _scheduler.begin(); i != _scheduler.end(); ++i)
        {
            if (i->object()->criterion() != IDLE)
            {
                i->object()->criterion().handle(event);
            }
        }
    }

    static int idle();

private:
    static void init();

protected:
    char *_stack;
    Context *volatile _context;

    volatile State _state;
    // Criterion _natural_priority;
    int _natural_priority;
    int criticalZonesCount = 0;

    SyncObject *_syncHolder = nullptr;
    SyncObject *_syncWaiter = nullptr;
    Queue *_waiting;
    Thread *volatile _joining;
    Queue::Element _link;

    static bool _not_booting;
    static volatile unsigned int _thread_count;
    static Scheduler_Timer *_timer;
    static Scheduler<Thread> _scheduler;
    static Spin _lock;
};

template <typename... Tn>
inline Thread::Thread(int (*entry)(Tn...), Tn... an)
    : _state(READY), _waiting(0), _joining(0), _link(this, NORMAL)
{
    db<Thread>(WRN) << "construtor da Thread\n"
                    << endl;
    constructor_prologue(STACK_SIZE);
    _context = CPU::init_stack(0, _stack + STACK_SIZE, &__exit, entry, an...);
    constructor_epilogue(entry, STACK_SIZE);
}

template <typename... Tn>
inline Thread::Thread(Configuration conf, int (*entry)(Tn...), Tn... an)
    : _state(conf.state), _waiting(0), _joining(0), _link(this, conf.criterion)
{
    db<Thread>(WRN) << "construtor da Thread\n"
                    << endl;
    constructor_prologue(conf.stack_size);
    _context = CPU::init_stack(0, _stack + conf.stack_size, &__exit, entry, an...);
    constructor_epilogue(entry, conf.stack_size);
}

// A Java-like Active Object
class Active : public Thread
{
public:
    Active() : Thread(Configuration(Thread::SUSPENDED), &entry, this) {}
    virtual ~Active() {}

    virtual int run() = 0;

    void start() { resume(); }

private:
    static int entry(Active *runnable) { return runnable->run(); }
};

// An event handler that triggers a thread (see handler.h)
class Thread_Handler : public Handler
{
public:
    Thread_Handler(Thread *h) : _handler(h) {}
    ~Thread_Handler() {}

    void operator()() { _handler->resume(); }

private:
    Thread *_handler;
};

__END_SYS

#endif
