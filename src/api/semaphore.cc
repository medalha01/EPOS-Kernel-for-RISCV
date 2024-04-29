// EPOS Semaphore Implementation

#include <synchronizer.h>

__BEGIN_SYS

Semaphore::Semaphore(long value, bool useCeiling, bool useInherintace) : _value(value), _hasCeiling(useCeiling), _inheritance(useInherintace)
{
    db<Synchronizer>(TRC) << "Semaphore(value=" << _value << ") => " << this << endl;
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

    if (fdec(_value) < 1)
    {
        _addResource(running_thread, &waiting_threads);
        if (_hasCeiling)
        {
            if (!_most_urgent_in_await)
                _most_urgent_in_await = _getHighestPriorityThread(&waiting_threads, false);

            if (_inheritance && _most_urgent_in_await)
                temp_priority = _most_urgent_in_await->criterion()._priority;
            Thread::start_periodic_critical(_lock_holder, incrementFlag, temp_priority);
            incrementFlag = false;
        }

        sleep();
        _removeResource(running_thread, &waiting_threads);
        if (_most_urgent_in_await == running_thread)
        {
            _most_urgent_in_await = nullptr;
            //_most_urgent_in_await = _getHighestPriorityThread(&waiting_threads, false);
        }
    }
    _addResource(running_thread, &resource_holder_list);

    if (!_lock_holder && _hasCeiling)
    {

        _lock_holder = _getHighestPriorityThread(&resource_holder_list, true);
        if (_value < 1 && _most_urgent_in_await)
        {
            if (_inheritance)
            {
                temp_priority = _most_urgent_in_await->criterion()._priority;
            }
            Thread::start_periodic_critical(_lock_holder, incrementFlag, temp_priority);
            incrementFlag = false;
        }
    }
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

    if (finc(_value) < 0)
        wakeup();
    incrementFlag = true;

    _removeResource(running_thread, &resource_holder_list);

    if (running_thread == _lock_holder)
        _lock_holder = _getHighestPriorityThread(&resource_holder_list, true);

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

Thread *Semaphore::_getHighestPriorityThread(List<Thread, List_Elements::Doubly_Linked<Thread>> *list, bool _hasToBeReady)
{
    auto highest_priority_thread = list->head();
    if (highest_priority_thread->object() == nullptr)
    {
        return nullptr;
    }
    auto next_thread_pointer = highest_priority_thread->next();

    while (next_thread_pointer != nullptr)
    {
        auto next_state = next_thread_pointer->object()->_state;
        bool isReady = (next_state == Thread::READY || next_state == Thread::RUNNING);
        bool isNextPriorityHigher = next_thread_pointer->object()->criterion()._priority < highest_priority_thread->object()->criterion()._priority;
        if (isNextPriorityHigher && ((_hasToBeReady && isReady) || !_hasToBeReady))
            highest_priority_thread = next_thread_pointer;

        next_thread_pointer = next_thread_pointer->next();
    }

    return highest_priority_thread->object();
}

__END_SYS
