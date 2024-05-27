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
CpuLookupTable Thread::_cpu_lookup_table;

Thread *volatile Thread::self()
{
	return _not_booting ? running() : reinterpret_cast<Thread *volatile>(CPU::id() + 1);
}

void Thread::constructor_prologue(unsigned int stack_size)
{
    lock();

    _thread_count++;
    _scheduler.insert(this);

    _stack = new (SYSTEM) char[stack_size];
}

void Thread::constructor_epilogue(Log_Addr entry, unsigned int stack_size)
{
    //db<Thread>(TRC) << "Thread(entry=" << entry
    //                << ",state=" << _state
    //                << ",priority=" << _link.rank()
    //                << ",stack={b=" << reinterpret_cast<void *>(_stack)
    //                << ",s=" << stack_size
    //                << "},context={b=" << _context
    //                << "," << *_context << "}) => " << this << endl;

    assert((_state != WAITING) && (_state != FINISHING)); // invalid states

    if ((_state != READY) && (_state != RUNNING))
    {
        _scheduler.suspend(this);
    }

    criterion().handle(Criterion::CREATE);

    if (preemptive && (_state == READY) && (_link.rank() != IDLE))
    {
        assert(locked());

        reschedule(_link.rank().queue());
    }

	// This is to be executed only by the MAIN thread when it is created.
	// This is such that it can skip the search of new CPU cores available,
	// since all cores will have ´nullptr´ as their executing thread,
	// i.e. all of them will be free. As such, no IRQ is needed, we just take the first one.
	if (CPU::is_smp() && _state != RUNNING) 
	{
		if (Traits<Thread>::smp_algorithm == Traits_Tokens::GLOBAL)
		{
			int target_core = _cpu_lookup_table.get_lowest_priority_core(_link.rank());
			//db<Thread>(WRN) << "chosen = " << target_core << endl;
			//_cpu_lookup_table.print_table();

			// If there is a suitable core (whether by lower priority, or running IDLE),
			// then target_core should be different then -1, and we send an interrupt to that core.
			if (target_core != -1) 
			{
				//db<Thread>(WRN) << "env int = " << IC::INT_RESCHEDULER << endl;
				reschedule(target_core);
			} 
		}
		else if (Traits<Thread>::smp_algorithm == Traits_Tokens::PARTITIONED)
		{
			unsigned int target_core = criterion().queue();		
			db<Thread>(WRN) << "cons target = " << target_core << endl;

			// There is already this check inside the reschedule method, but we can
			// double it here because if the current core is the same as the target,
			// then we would rather just end the constructor normally, as would
			// usually happen without these new additions.
			if (target_core != CPU::id())
			{
				db<Thread>(WRN) << "cons send to c = " << target_core << endl;
				reschedule(target_core);
			}
		}
	}

	// As said previously, if MAIN is being constructed here, we can just
	// set the CPU core in which it is in to MAIN's priority, and keep going.
	if (CPU::is_smp() && _state == RUNNING) 
	{
		_cpu_lookup_table.set_thread_on_cpu(running());
		_cpu_lookup_table.print_table();
	}

    unlock();
}

Thread::~Thread()
{
    lock();

    //db<Thread>(TRC) << "~Thread(this=" << this
    //                << ",state=" << _state
    //                << ",priority=" << _link.rank()
    //                << ",stack={b=" << reinterpret_cast<void *>(_stack)
    //                << ",context={b=" << _context
    //                << "," << *_context << "})" << endl;

    // The running thread cannot delete itself!
    assert(_state != RUNNING);

	// TODO: @arthur remover thread da lookup table do core se estiver lá

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

int Thread::join()
{
    lock();

    //db<Thread>(TRC) << "Thread::join(this=" << this << ",state=" << _state << ")" << endl;

    // Precondition: no Thread::self()->join()
    assert(running() != this);

    // Precondition: a single joiner
    assert(!_joining);

	// TODO: @arthur talvez adicionar coisa do PLLF aqui
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

    //db<Thread>(TRC) << "Thread::pass(this=" << this << ")" << endl;

    Thread *prev = running();
    Thread *next = _scheduler.choose(this);

    if (next)
	{
        dispatch(prev, next, false);
	}

    unlock();
}

void Thread::suspend()
{
    lock();

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

    if (_state == SUSPENDED)
    {
        _state = READY;
        _scheduler.resume(this);

        if (preemptive)
		{
			if (Traits<Thread>::smp_algorithm == Traits_Tokens::GLOBAL)
			{
				int target_core = _cpu_lookup_table.get_lowest_priority_core(_link.rank());
				//_cpu_lookup_table.print_table();

				if (target_core != -1)
				{
					db<Thread>(WRN) << "[=] rs to c = " << target_core << endl;
					reschedule(target_core);
				}
				else 
				{
					db<Thread>(WRN) << "[=] rs to same = " << CPU::id() << endl;
					//db<Thread>(WRN) << ">wr p = " << _link.rank() << endl;
					//_cpu_lookup_table.print_table();
					reschedule();
				}

			}
			else if (Traits<Thread>::smp_algorithm == Traits_Tokens::PARTITIONED)
			{
				int target_core = this->criterion().queue();

				db<Thread>(WRN) << "[=] prs to c = " << target_core << endl;
				reschedule(target_core);
			}
			else 
			{
				reschedule();
			}
		}
    }
    //else
    //    db<Thread>(WRN) << "Resume called for unsuspended object!" << endl;

    unlock();
}

void Thread::yield()
{
    lock();

    //db<Thread>(TRC) << "Thread::yield(running=" << running() << ")" << endl;

    Thread *prev = running();
    Thread *next = _scheduler.choose_another();

    dispatch(prev, next);

    unlock();
}

void Thread::exit(int status)
{
    lock();

    //db<Thread>(TRC) << "Thread::exit(status=" << status
	//	<< ") [running=" << running() << "]" << endl;

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
    //db<Thread>(TRC) << "Thread::sleep(running=" << running() << ",q=" << q << ")" << endl;

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
    //db<Thread>(TRC) << "Thread::wakeup(running="
	//	<< running() << ",q=" << q << ")" << endl;

	//db<Thread>(WRN) << "wu:)" << endl;

    assert(locked()); // locking handled by caller

    if (!q->empty())
    {
        Thread *t = q->remove()->object();
        t->_state = READY;
        t->_waiting = 0;
        _scheduler.resume(t);

		if (preemptive)
		{
			if (Traits<Thread>::smp_algorithm == Traits_Tokens::GLOBAL) 
			{
				int target_core = _cpu_lookup_table.get_lowest_priority_core(t->_link.rank());
				_cpu_lookup_table.print_table();

				if (target_core != -1) 
				{
					//db<Thread>(WRN) << "[=] wu to c = " << target_core << endl;
					reschedule(target_core);
				}
				else 
				{
					//db<Thread>(WRN) << "[=] wu to same = " << CPU::id() << endl;
					reschedule();
				}
			}
			else if (Traits<Thread>::smp_algorithm == Traits_Tokens::PARTITIONED)
			{
				int target_core = t->criterion().queue();

				db<Thread>(WRN) << "[=] pwu to c = " << target_core << endl;
				reschedule(target_core);
			}
			else 
			{
				reschedule();
			}
        }
    }
}

void Thread::wakeup_all(Queue *q)
{
    //db<Thread>(TRC) << "Thread::wakeup_all(running=" << running() << ",q=" << q << ")" << endl;

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

void Thread::prioritize(Queue *q)
{
    assert(locked()); // locking handled by caller

    if (priority_inversion_protocol == Traits<Build>::NONE)
    {
        return;
    }

    //db<Thread>(TRC) << "Thread::prioritize(q=" << q << ") [running=" << running() << "]" << endl;

    Thread *r = running();
    for (Queue::Iterator i = q->begin(); i != q->end(); ++i)
    {
        if (i->object()->priority() > r->priority())
        {
            r->_natural_priority = r->criterion();
            Criterion c = (priority_inversion_protocol == Traits<Build>::CEILING)
				? CEILING : r->criterion();
            if (r->_state == READY)
            {
				// TODO: talvez tenha que dar remove 
				// e insert aqui que nem anteriormente
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

void Thread::deprioritize(Queue *q)
{
    assert(locked()); // locking handled by caller

    if (priority_inversion_protocol == Traits<Build>::NONE)
        return;

    //db<Thread>(TRC) << "Thread::deprioritize(q=" << q << ") [running=" << running() << "]" << endl;

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

void Thread::int_rescheduler(IC::Interrupt_Id i)
{
	db<Thread>(WRN) << "int_resch" << endl;
    lock();
    reschedule();
    unlock();
}

void Thread::reschedule(unsigned int cpu)
{
    //if (!Criterion::timed || Traits<Thread>::hysterically_debugged)
    //    db<Thread>(TRC) << "Thread::reschedule()" << endl;

    assert(locked()); // locking handled by caller

	if (!Traits<Machine>::multi || CPU::id() == cpu)
	{
		Thread *prev = running();
		Thread *next = _scheduler.choose();

		db<Thread>(WRN) << "tr p = " << prev->criterion()
			<< " n = " << next->criterion() << endl;

		dispatch(prev, next);
	}
	else 
	{
		db<Thread>(WRN) << "env " << CPU::id() << " n = " << cpu << endl;

		// Ensures that no two cores send interrupts to the very same CPU,
		// from both seeing that it is running a lower priority thread than
		// the current one. This might happen, albeit rarely, so this is needed.
		_cpu_lookup_table.already_dispatched(cpu);
		//_cpu_lookup_table.print_table();

		IC::ipi(cpu, IC::INT_RESCHEDULER);			
	}
}

void Thread::time_slicer(IC::Interrupt_Id i)
{
	//db<Thread>(WRN) << "------->int time slicer<-------" << endl;
    lock();
    reschedule();
    unlock();
}

void Thread::dispatch(Thread *prev, Thread *next, bool charge)
{
    // "next" is not in the scheduler's queue anymore. It's already "chosen"

	//db<Thread>(WRN) << "t d" << endl;
	//_cpu_lookup_table.set_thread_on_cpu(next);

	db<Thread>(WRN) << "[=] set on dispatch" << endl;
	_cpu_lookup_table.set_thread_on_cpu(next);

    if (charge && Criterion::timed)
    {
        _timer->restart(); // scheduler_timer
    }

    if (prev != next)
    {
		// This print is enough to cause lots of problems.
		//_cpu_lookup_table.print_table();

        if (Criterion::dynamic)
        {
            prev->criterion().handle(Criterion::CHARGE | Criterion::LEAVE);
            for_all_threads(Criterion::UPDATE);
            next->criterion().handle(Criterion::AWARD | Criterion::ENTER);
        }

        if (prev->_state == RUNNING)
            prev->_state = READY;
        next->_state = RUNNING;

        if (CPU::is_smp())
        {
            _lock.release();
        }

        // The non-volatile pointer to volatile pointer to a non-volatile context is correct
        // and necessary because of context switches, but here, we are locked() and
        // passing the volatile to switch_context forces it to push prev onto the stack,
        // disrupting the context (it doesn't make a difference for Intel, which already saves
        // parameters on the stack anyway).
        CPU::switch_context(const_cast<Context **>(&prev->_context), next->_context);

        if (CPU::is_smp())
        {
            _lock.acquire();
        }
    }
}

int Thread::idle()
{
	// Resets the value in the lookup table, effectively saying that this is IDLE.
	// By convention of our own, nullptr = idle in the lookup table.
	_cpu_lookup_table.clear_cpu(CPU::id());

	// someone else besides idle
    while (_thread_count > CPU::cores())
    { 
        db<Thread>(WRN) << "Halting the machine ..." << endl;
		_cpu_lookup_table.clear_cpu(CPU::id());
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

    //for (;;) ;

    return 0;
}

__END_SYS
