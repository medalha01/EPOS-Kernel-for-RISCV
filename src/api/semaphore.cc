// EPOS Semaphore Implementation

#include <synchronizer.h>

__BEGIN_SYS

Semaphore::Semaphore(long value, bool useCeiling, bool useInherintace) : _value(value), _hasCeiling(useCeiling), _inheritance(useInherintace)
{
    db<Synchronizer>(TRC) << "Semaphore(value=" << _value << ") => " << this << endl;
    Thread *threadHolder[value] = {nullptr};
    resource_holders = threadHolder;
}

Semaphore::~Semaphore()
{
    db<Synchronizer>(TRC) << "~Semaphore(this=" << this << ")" << endl;
}

void Semaphore::p()
{
    Thread *running_thread = Thread::running();

    int temp_priority = Thread::CEILING;
    db<Synchronizer>(TRC)
        << "Semaphore::p(this=" << this << ",value=" << _value << ")" << endl;
    begin_atomic();

    if (_inheritance)

        temp_priority = running_thread->criterion()._priority;

    if (fdec(_value) < 1)
    {
        _addResource(running_thread, &waiting_threads);
        if (_hasCeiling)
        {
            Thread::start_periodic_critical(_lock_holder, incrementFlag, temp_priority);
            incrementFlag = false;
        }

        sleep();
        _removeResource(running_thread, &waiting_threads);
    }
    _addResource(running_thread, &resource_holder_list);

    if (!_lock_holder)
        _lock_holder = running_thread;
    end_atomic();
}

void Semaphore::v()
{
    Thread *running_thread = Thread::running();

    db<Synchronizer>(TRC)
        << "Semaphore::v(this=" << this << ",value=" << _value << ")" << endl;

    begin_atomic();
    // running_thread->_number_of_critical_locks--;
    if (_hasCeiling)
        Thread::end_periodic_critical(running_thread, incrementFlag);
    if (running_thread == _lock_holder)
        _lock_holder = nullptr;
    if (finc(_value) < 0)
        wakeup();
    incrementFlag = true;

    _removeResource(running_thread, &resource_holder_list);

    end_atomic();
}

void Semaphore::_addResource(Thread *t, List<Thread, List_Elements::Doubly_Linked<Thread>> *list)
{
    if (t)
    {

        List_Elements::Doubly_Linked<Thread> *rt_object = new List_Elements::Doubly_Linked<Thread>(t);
        list->insert(rt_object);
    }
}

void Semaphore::_removeResource(Thread *t, List<Thread, List_Elements::Doubly_Linked<Thread>> *list)
{
    if (t)
    {

        List_Elements::Doubly_Linked<Thread> *rt_object = list->remove(t);
        if (rt_object)
            delete rt_object;
    }
}

__END_SYS
