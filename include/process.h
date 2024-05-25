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

class CpuLookupTable;

class Thread
{
    friend class Init_End;            // context->load()
    friend class Init_System;         // for init() on CPU != 0
    friend class Scheduler<Thread>;   // for link()
    friend class Synchronizer_Common; // for lock() and sleep()
    friend class Alarm;               // for lock()
    friend class System;              // for init()
    friend class IC;                  // for link() for priority ceiling
    friend void ::_lock_heap();       // for lock()
    friend void ::_unlock_heap();     // for unlock()
    friend class CpuLookupTable;

protected:
    static const bool preemptive = Traits<Thread>::Criterion::preemptive;
    static const int priority_inversion_protocol = Traits<Thread>::priority_inversion_protocol;
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

    static void prioritize(Queue *queue);
    static void deprioritize(Queue *queue);

    //static void reschedule();
    static void reschedule(unsigned int cpu = CPU::id());
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
    Criterion _natural_priority;
    Queue *_waiting;
    Thread *volatile _joining;
    Queue::Element _link;

    static bool _not_booting;
    static volatile unsigned int _thread_count;
    static Scheduler_Timer *_timer;
    static Scheduler<Thread> _scheduler;
    static Spin _lock;
	static CpuLookupTable _cpu_lookup_table;
};

template <typename... Tn>
inline Thread::Thread(int (*entry)(Tn...), Tn... an)
    : _state(READY), _waiting(0), _joining(0), _link(this, NORMAL)
{
    constructor_prologue(STACK_SIZE);
    _context = CPU::init_stack(0, _stack + STACK_SIZE, &__exit, entry, an...);
    constructor_epilogue(entry, STACK_SIZE);
}

template <typename... Tn>
inline Thread::Thread(Configuration conf, int (*entry)(Tn...), Tn... an)
    : _state(conf.state), _waiting(0), _joining(0), _link(this, conf.criterion)
{
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

class CpuLookupTable 
{
    friend class Thread;
    friend class Alarm;
    friend class Mutex;
    friend class Condition;
    friend class Semaphore;
    friend class Segment;

    typedef Traits<Thread>::Criterion Criterion;

private:
    Criterion *threads_criterion_on_execution[Traits<Build>::CPUS];
    Thread *running_thread_by_core[Traits<Build>::CPUS];

public:
    CpuLookupTable()
    {
        // Initialize arrays
        for (unsigned int i = 0; i < Traits<Build>::CPUS; i++)
        {
            running_thread_by_core[i] = nullptr;
            threads_criterion_on_execution[i] = nullptr;
        }
    }

	void print_table()
	{
		bool tmp_locked = false;

		if (!Thread::locked())
		{
			Thread::lock();
			tmp_locked = true;
		}

		db<Thread>(WRN) << "Printing CPU lookup table..." << endl;

		for (unsigned int i = 0; i < CPU::cores(); i++)
		{
			Thread * thread      = running_thread_by_core[i];
			Criterion * priority = threads_criterion_on_execution[i];

			db<Thread>(WRN) << "[" << i << "] -> {Thread = " 
							<< thread << ", Priority = " 
							<< *priority << "}" << endl;
		}

		db<Thread>(WRN) << "End printing CPU lookup table..." << endl;

		if (tmp_locked) 
		{
			Thread::unlock();
		}
	}

    Thread * get_thread_on_cpu(unsigned int cpu_id)
    {
        assert(cpu_id < CPU::cores()); // Ensure valid cpu_id
        return running_thread_by_core[cpu_id];
    }

    Criterion * get_priority_on_cpu(unsigned int cpu_id)
    {
        assert(cpu_id < CPU::cores()); // Ensure valid cpu_id
        return threads_criterion_on_execution[cpu_id];
    }

    //void set_thread_on_cpu_table(Thread *running, unsigned int id)
    //{
    //    assert(id < Traits<Build>::CPUS); // Ensure valid cpu_id
    //    running_thread_by_core[id] = running;
    //    threads_criterion_on_execution[id] = &running->criterion();
    //}

	int get_interruptible_core(unsigned int current_priority)
	{
		int id = get_idle_cpu();	
		if (id == -1)
		{
			id = get_cpu_with_lowest_priority();	
		}
		return id;
	}

    void set_thread_on_cpu(Thread *running)
    {
        unsigned int id = CPU::id();

        running_thread_by_core[id] = running;
        threads_criterion_on_execution[id] = &running->criterion();

		db<Thread>(WRN) << "updated thread = " << running 
						<< " at cpu = " << id << endl;
    }

	//unsigned int 

    int get_cpu_with_lowest_priority()
    {
        int min = Thread::IDLE;
        unsigned int lowest_priority_cpu = 0;
        for (unsigned int i = 0; i < Traits<Build>::CPUS; i++)
        {
            if (min > *threads_criterion_on_execution[i])
            {
                min = *threads_criterion_on_execution[i];
                lowest_priority_cpu = i;
            }
        }

        return lowest_priority_cpu;
    }

    int get_idle_cpu()
    {
        for (unsigned int i = 0; i < Traits<Build>::CPUS; i++)
        {
            if ((running_thread_by_core[i] == nullptr) or (*threads_criterion_on_execution[i] == Thread::IDLE))
            {
                return i;
            }
        }
        return -1;
    }

    void clear_cpu(unsigned int cpu_id)
    {
        assert(cpu_id < Traits<Build>::CPUS); // Ensure valid cpu_id
        running_thread_by_core[cpu_id] = nullptr;
        threads_criterion_on_execution[cpu_id] = nullptr;
    }

    // Check if a specific CPU is idle
    bool is_cpu_idle(unsigned int cpu_id)
    {
        assert(cpu_id < Traits<Build>::CPUS); // Ensure valid cpu_id
        return (running_thread_by_core[cpu_id] == nullptr) ||
               (threads_criterion_on_execution[cpu_id] && *threads_criterion_on_execution[cpu_id] == Thread::IDLE);
    }

    // Check if all CPUs are idle
    bool are_all_cpus_idle()
    {
        for (unsigned int i = 0; i < Traits<Build>::CPUS; ++i)
        {
            if (!is_cpu_idle(i))
            {
                return false;
            }
        }
        return true;
    }
};

__END_SYS

#endif
