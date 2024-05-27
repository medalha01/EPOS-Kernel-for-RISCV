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
        Configuration(State s = READY, Criterion c = NORMAL, 
				unsigned int ss = STACK_SIZE)
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

    static volatile bool locked() 
	{
		return (Traits<Machine>::multi)
			? _lock.taken() : CPU::int_disabled(); 
	}

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
				db<Thread>(WRN) << "updating " << i->object()->criterion() << ", ";
                i->object()->criterion().handle(event);
				db<Thread>(WRN) << "now = " << i->object()->criterion() << endl;
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

/* A lookup table that maps cores and their currently running threads.
 *
 * This is so we can know what cores should be interrupted:
 * those whose running threads have a lower priority than 
 * the current one being scheduled. */
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
    Thread * _running_thread_by_core[Traits<Build>::CPUS];
	bool _already_dispatched[Traits<Build>::CPUS];

public:
    CpuLookupTable()
    {
        // Initialize arrays
        for (unsigned int i = 0; i < CPU::cores(); i++)
        {
            _running_thread_by_core[i] = nullptr;
			_already_dispatched[i] = false;
        }
    }

	/* Helper method for printing the values of the CPU lookup table, for debugging purposes. */
	void print_table()
	{
		//db<Thread>(WRN) << "Printing CPU lookup table..." << endl;

		for (unsigned int i = 0; i < CPU::cores(); i++)
		{
			Thread * thread = _running_thread_by_core[i];

			db<Thread>(WRN) << "[" << i << "] -> ";

			if (thread == nullptr) 
			{
				db<Thread>(WRN) << "nullptr / " << _already_dispatched[i] << endl;
				continue;
			}
			
			Criterion * priority = &thread->criterion();

			db<Thread>(WRN) << *priority << " / " << _already_dispatched[i] << endl;
		}

		//db<Thread>(WRN) << "End printing CPU lookup table..." << endl;
	}

	/* Updates the lookup table with currently running thread. 
	 * This method is meant to be used whenever we change contexts in a given core,
	 * so as to always keep an updated list of possible interrupt targets.
	 *
	 * This also resets the _already_dispatched[id] flag. 
	 * See the next method for more details. */
    void set_thread_on_cpu(Thread *running)
    {
        unsigned int id = CPU::id();

		_already_dispatched[id] = false;
        _running_thread_by_core[id] = running;
    }

	/* Assigns a temporary flag to an array in the position corresponding to the CPU id.
	 * This is done to prevent the rare, but perhaps possible event of two cores finding out
	 * about the same core that runs a thread with a lower priority, since the two would send
	 * an interrupt to that core. 
	 *
	 * Note that the window for this to happen in effectively so minimal that it is near impossible,
	 * because it spans from receiving the interrupt, going to the dispatch method, int_rescheduler (int_not)
	 * and then into the Thread::reschedule() method, which finally would put a lock in place. 
	 *
	 * Unlikely for most cases, but possible. So we set the flag as `true` when a core already picked this target,
	 * and we reset it to `false` during the set_thread_on_cpu() method, usually during Thread::dispatch() */
	void already_dispatched(unsigned int cpu)
	{
		assert(Thread::locked());
		_already_dispatched[cpu] = true;
	}

	/* Finds out if there is a valid target for an INT_RESCHEDULER interrupt:
	 * if there is a core running a lower priority thread, or if there is a core running idle. */
	int get_lowest_priority_core(int current_priority = (1 << 31)) 
	{
		//print_table();

		if (current_priority == Thread::IDLE) return -1;

		int min = current_priority;
		int chosen = -1;

		for (unsigned int i = 0; i < CPU::cores(); i++)
		{
			if (_running_thread_by_core[i] == nullptr)
			{
				return i;
			}

			Criterion * criterion = &_running_thread_by_core[i]->criterion();

			if (*criterion > min && !_already_dispatched[i])
			{
				min = *criterion;
				chosen = i;
			}
		}

		//print_table();

		return chosen;
	}

	/* Just resets the entry corresponting to the `cpu_id` by setting it to nullptr.
	 * This ensures that this core will easily be picked as a target for interrupt,
	 * when it enters Thread::idle(). */
    void clear_cpu(unsigned int cpu_id)
    {
		db<Thread>(WRN) << "clt clear cpu = " << cpu_id << endl;

        assert(cpu_id < CPU::cores()); // Ensure valid cpu_id
        _running_thread_by_core[cpu_id] = nullptr;
    }
};

__END_SYS

#endif
