// EPOS Thread Implementation

#include <machine.h>
#include <system.h>
#include <process.h>

extern "C"
{
    volatile unsigned long _running() __attribute__((alias("_ZN4EPOS1S6Thread4selfEv")));
}

__BEGIN_SYS

extern OStream kout;

volatile unsigned int Thread::_thread_count;
Scheduler_Timer *Thread::_timer;
Scheduler<Thread> Thread::_scheduler;
Spin Thread::_lock;
bool Thread::_not_booting;

Thread *volatile Thread::self()
{
    {
        return _not_booting ? running() : reinterpret_cast<Thread *volatile>(CPU::id() + 1);
    }
}
void Thread::constructor_prologue(unsigned int stack_size)
{
    db<Thread>(WRN) << "CONS init" << endl;

    lock();

    db<Thread>(WRN) << "CONS post lock" << endl;

    _thread_count++;
    _scheduler.insert(this);

    db<Thread>(WRN) << "CONS post scheduler insert" << endl;

    _stack = new (SYSTEM) char[stack_size];

    db<Thread>(WRN) << "constructor_prologue post-stack insert" << endl;
}

void Thread::constructor_epilogue(Log_Addr entry, unsigned int stack_size)
{
    db<Thread>(TRC) << "Thread(entry=" << entry
                    << ",state=" << _state
                    << ",priority=" << _link.rank()
                    << ",stack={b=" << reinterpret_cast<void *>(_stack)
                    << ",s=" << stack_size
                    << "},context={b=" << _context
                    << "," << *_context << "}) => " << this << endl;

    db<Thread>(WRN) << "constructor_epilogue start" << endl;

    assert((_state != WAITING) && (_state != FINISHING)); // invalid states

    if ((_state != READY) && (_state != RUNNING))
    {
        db<Thread>(WRN) << "--segundo if do epilogue = " << CPU::id() << endl;
        _scheduler.suspend(this);
    }

    criterion().handle(Criterion::CREATE);

    if (preemptive && (_state == READY) && (_link.rank() != IDLE))
    {
        db<Thread>(WRN) << "---terceiro if do epilogue = " << CPU::id() << endl;
        assert(locked());
        reschedule(_link.rank().queue());
        db<Thread>(WRN) << "---terceiro if, depois do reschedule" << endl;
    }

    unlock();
    db<Thread>(WRN) << "-------constructor_epilogue post-unlock" << endl;
}

Thread::~Thread()
{
    lock();

    db<Thread>(TRC) << "~Thread(this=" << this
                    << ",state=" << _state
                    << ",priority=" << _link.rank()
                    << ",stack={b=" << reinterpret_cast<void *>(_stack)
                    << ",context={b=" << _context
                    << "," << *_context << "})" << endl;

    // The running thread cannot delete itself!
    assert(_state != RUNNING);

    switch (_state)
    {
    case RUNNING: // For switch completion only: the running thread would have deleted itself! Stack wouldn't have been released!
        exit(-1);
        break;
    case READY:
        _scheduler.remove(this);
        _thread_count--;
        break;
    case SUSPENDED:
        _scheduler.resume(this);
        _scheduler.remove(this);
        _thread_count--;
        break;
    case WAITING:
        _waiting->remove(this);
        _scheduler.resume(this);
        _scheduler.remove(this);
        _thread_count--;
        break;
    case FINISHING: // Already called exit()
        break;
    }

    if (_joining)
    {
        _joining->resume();
    }

    unlock();

    delete _stack;
}
void Thread::priority(Criterion c)
{
    lock();

    db<Thread>(TRC) << "Thread::priority(this=" << this << ",prio=" << c << ")" << endl;

    unsigned long old_cpu = _link.rank().queue();
    unsigned long new_cpu = c.queue();

    if (_state != RUNNING)
    { // reorder the scheduling queue
        _scheduler.remove(this);
        _link.rank(c);
        _scheduler.insert(this);
    }
    else
        _link.rank(c);

    if (preemptive)
    {
        if (Traits<Machine>::multi)
        {
            assert(locked());

            if (old_cpu != CPU::id())
                reschedule(old_cpu);
            if (new_cpu != CPU::id())
                reschedule(new_cpu);
        }
        else
        {
            assert(locked());

            reschedule();
        }
    }

    unlock();
}

int Thread::join()
{
    lock();

    db<Thread>(TRC) << "Thread::join(this=" << this << ",state=" << _state << ")" << endl;

    // Precondition: no Thread::self()->join()
    assert(running() != this);

    // Precondition: a single joiner
    assert(!_joining);

    if (_state != FINISHING)
    {
        Thread *prev = running();

        _joining = prev;
        prev->_state = SUSPENDED;
        _scheduler.suspend(prev); // implicitly choose() if suspending chosen()

        Thread *next = _scheduler.chosen();

        dispatch(prev, next);
    }

    unlock();

    return *reinterpret_cast<int *>(_stack);
}

void Thread::pass()
{
    lock();

    db<Thread>(TRC) << "Thread::pass(this=" << this << ")" << endl;

    Thread *prev = running();
    Thread *next = _scheduler.choose(this);

    if (next)
        dispatch(prev, next, false);
    else
        db<Thread>(WRN) << "Thread::pass => thread (" << this << ") not ready!" << endl;

    unlock();
}

void Thread::suspend()
{
    lock();

    db<Thread>(TRC) << "Thread::suspend(this=" << this << ")" << endl;

    Thread *prev = running();

    _state = SUSPENDED;
    _scheduler.suspend(this);

    Thread *next = _scheduler.chosen();

    dispatch(prev, next);

    unlock();
}

void Thread::resume()
{
    lock();

    db<Thread>(TRC) << "Thread::resume(this=" << this << ")" << endl;

    if (_state == SUSPENDED)
    {
        _state = READY;
        _scheduler.resume(this);

        if (preemptive)
            reschedule(_link.rank().queue());
    }
    else
        db<Thread>(WRN) << "Resume called for unsuspended object!" << endl;

    unlock();
}

void Thread::yield()
{
    lock();

    db<Thread>(TRC) << "Thread::yield(running=" << running() << ")" << endl;

    Thread *prev = running();
    Thread *next = _scheduler.choose_another();

    dispatch(prev, next);

    unlock();
}

void Thread::exit(int status)
{
    lock();

    db<Thread>(TRC) << "Thread::exit(status=" << status << ") [running=" << running() << "]" << endl;

    Thread *prev = running();
    _scheduler.remove(prev);
    prev->_state = FINISHING;
    *reinterpret_cast<int *>(prev->_stack) = status;
    prev->criterion().handle(Criterion::FINISH);

    _thread_count--;

    if (prev->_joining)
    {
        prev->_joining->_state = READY;
        _scheduler.resume(prev->_joining);
        prev->_joining = 0;
    }

    Thread *next = _scheduler.choose(); // at least idle will always be there

    dispatch(prev, next);

    unlock();
}

void Thread::sleep(Queue *q)
{
    db<Thread>(TRC) << "Thread::sleep(running=" << running() << ",q=" << q << ")" << endl;

    assert(locked()); // locking handled by caller

    Thread *prev = running();
    _scheduler.suspend(prev);
    prev->_state = WAITING;
    prev->_waiting = q;
    q->insert(&prev->_link);

    Thread *next = _scheduler.chosen();

    dispatch(prev, next);
}

void Thread::wakeup(Queue *q)
{
    db<Thread>(TRC) << "Thread::wakeup(running=" << running() << ",q=" << q << ")" << endl;

    assert(locked()); // locking handled by caller

    if (!q->empty())
    {
        Thread *t = q->remove()->object();
        t->_state = READY;
        t->_waiting = 0;
        _scheduler.resume(t);

        if (preemptive)
        {
            reschedule(t->_link.rank().queue());
        }
    }
}

void Thread::wakeup_all(Queue *q)
{
    db<Thread>(TRC) << "Thread::wakeup_all(running=" << running() << ",q=" << q << ")" << endl;

    assert(locked()); // locking handled by caller

    if (!q->empty())
    {
        assert(Criterion::QUEUES <= sizeof(unsigned long) * 8);
        unsigned long cpus = 0;

        while (!q->empty())
        {
            Thread *t = q->remove()->object();
            t->_state = READY;
            t->_waiting = 0;
            _scheduler.resume(t);
            cpus |= 1 << t->_link.rank().queue();
        }

        if (preemptive)
        {
            for (unsigned long i = 0; i < Criterion::QUEUES; i++)
            {
                if (cpus & (1 << i))
                {
                    reschedule(i);
                }
            }
        }
    }
}

void Thread::reset_protocol()
{
    Criterion *c = &criterion();
    c->_locked = false;
    c->_priority = _natural_priority;
    c->handle(Criterion::UPDATE);

    if (this->_state == READY)
    {
        _scheduler.suspend(this);
        this->_link.rank(*c);
        _scheduler.resume(this);
    }
    else if (this->state() == WAITING)
    {
        this->_waiting->remove(&this->_link);
        this->_link.rank(*c);
        this->_waiting->insert(&this->_link);
    }
    else
    {
        this->_link.rank(*c);
    }
}

void Thread::raise_priority(int priority)
{
    assert(locked()); // locking handled by caller

    /*Thread::Criterion *thread_criterion = &current->object()->threadPointer->criterion();

    // Check if the thread's priority is higher than the new highest priority
    if (thread_criterion->_priority > highest_priority)
    {
        // If the thread's priority is not locked, update its natural priority
        if (!thread_criterion->_locked)
        {
            current->object()->threadPointer->_natural_priority = thread_criterion->_priority;
        }

        // Set the thread's priority to the new highest priority and lock it
        thread_criterion->_priority = priority;
        thread_criterion->_locked = true;
    }*/

    Thread::Criterion *thread_criterion = &this->criterion();

    if (this->priority() > priority)
    {
        if (!thread_criterion->_locked)
        {
            this->_natural_priority = thread_criterion->_priority;
        }
        kout << "New Priority is: " << priority << endl;
        thread_criterion->_priority = priority;
        thread_criterion->_locked = true;

        if (this->_state == READY)
        {
            _scheduler.suspend(this);
            this->_link.rank(*thread_criterion);
            _scheduler.resume(this);
        }
        else if (this->state() == WAITING)
        {
            this->_waiting->remove(&this->_link);
            this->_link.rank(*thread_criterion);
            this->_waiting->insert(&this->_link);
        }
        else
        {
            this->_link.rank(*thread_criterion);
        }
    }
}

void Thread::deprioritize(Queue *q)
{
    assert(locked()); // locking handled by caller

    if (priority_inversion_protocol == Traits<Build>::NONE)
        return;

    db<Thread>(TRC) << "Thread::deprioritize(q=" << q << ") [running=" << running() << "]" << endl;

    Thread *r = running();
    Criterion c = r->_natural_priority;
    for (Queue::Iterator i = q->begin(); i != q->end(); ++i)
    {
        if (i->object()->priority() != c)
        {
            if (r->_state == READY)
            {
                _scheduler.suspend(r);
                r->_link.rank(c);
                _scheduler.resume(r);
            }
            else if (r->state() == WAITING)
            {
                r->_waiting->remove(&r->_link);
                r->_link.rank(c);
                r->_waiting->insert(&r->_link);
            }
            else
            {
                r->_link.rank(c);
            }
        }
    }
}

void Thread::reschedule(unsigned int cpu)
{

    if (!Criterion::timed || Traits<Thread>::hysterically_debugged)
    {
        db<Thread>(TRC) << "Thread::reschedule()" << cpu << endl;
    }

    assert(locked()); // locking handled by caller

    if (!Traits<Machine>::multi || CPU::id() == cpu)
    {
        reschedule();
    }
    else
    {

        IC::ipi(cpu, IC::INT_RESCHEDULER);
    }
}

void Thread::int_rescheduler(IC::Interrupt_Id i)
{
    lock();
    reschedule();
    unlock();
}

void Thread::reschedule()
{

    if (!Criterion::timed || Traits<Thread>::hysterically_debugged)
        db<Thread>(TRC) << "Thread::reschedule()" << endl;

    assert(locked()); // locking handled by caller

    Thread *prev = running();
    Thread *next = _scheduler.choose();

    dispatch(prev, next);
}

void Thread::time_slicer(IC::Interrupt_Id i)
{
    lock();
    reschedule();
    unlock();
}

void Thread::dispatch(Thread *prev, Thread *next, bool charge)
{
    // "next" is not in the scheduler's queue anymore. It's already "chosen"

    if (charge && Criterion::timed)
    {
        _timer->restart(); // scheduler_timer
    }

    if (prev != next)
    {
        if (Criterion::dynamic)
        {
            prev->criterion().handle(Criterion::CHARGE | Criterion::LEAVE);
            for_all_threads(Criterion::UPDATE);
            next->criterion().handle(Criterion::AWARD | Criterion::ENTER);
        }

        if (prev->_state == RUNNING)
            prev->_state = READY;
        next->_state = RUNNING;

        if (Traits<Thread>::debugged && Traits<Debug>::info)
        {
            CPU::Context tmp;
            tmp.save();
            db<Thread>(INF) << "Thread::dispatch:prev={" << prev << ",ctx=" << tmp << "}" << endl;
        }

        db<Thread>(INF) << "Thread::dispatch:next={" << next << ",ctx=" << *next->_context << "}" << endl;

        if (Traits<Machine>::multi)
        {
            _lock.release();
        }

        // The non-volatile pointer to volatile pointer to a non-volatile context is correct
        // and necessary because of context switches, but here, we are locked() and
        // passing the volatile to switch_constext forces it to push prev onto the stack,
        // disrupting the context (it doesn't make a difference for Intel, which already saves
        // parameters on the stack anyway).
        CPU::switch_context(const_cast<Context **>(&prev->_context), next->_context);

        if (Traits<Machine>::multi)
        {
            _lock.acquire();
        }
    }
}

int Thread::idle()
{
    db<Thread>(TRC) << "Thread::idle(this=" << running() << ")" << endl;

    while (_thread_count > CPU::cores())
    { // someone else besides idle
        if (Traits<Thread>::trace_idle)
        {
            db<Thread>(TRC) << "Thread::idle(cpu=" << CPU::id() << ",this=" << running() << ")" << endl;
        }

        db<Thread>(WRN) << "Halting the machine ..." << endl;
        CPU::int_enable();
        CPU::halt();

        // a thread might have been woken up by another CPU
        if (_scheduler.schedulables() > 0)
        {
            yield();
        }
    }

    CPU::int_disable();

    if (CPU::is_bootstrap())
    {
        kout << "\n\n*** The last thread under control of EPOS has finished." << endl;
        kout << "*** EPOS is shutting down!" << endl;
        Machine::reboot();
    }

    for (;;)
        ;

    return 0;
}

__END_SYS
