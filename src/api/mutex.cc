// EPOS Mutex Implementation

#include <synchronizer.h>

__BEGIN_SYS

Mutex::Mutex() : _locked(false)
{
    db<Synchronizer>(TRC) << "Mutex() => " << this << endl;
}

Mutex::~Mutex()
{
    db<Synchronizer>(TRC) << "~Mutex(this=" << this << ")" << endl;
}

void Mutex::lock()
{
    db<Synchronizer>(TRC) << "Mutex::lock(this=" << this << ")" << endl;
    lock_for_acquiring();
    Thread *exec_thread = Thread::self();

    if (tsl(_locked))
        sleep();
    unlock_for_acquiring();

    Queue_Element *sync_obj = resource_holder_list.head();
    bool thread_is_present = false;
    while (sync_obj != nullptr)
    {
        if (exec_thread == sync_obj->object()->tp)
        {
            thread_is_present = true;
        }
        sync_obj = sync_obj->next();
    }

    if (!thread_is_present)
    {
        Sync_Object resource = Sync_Object(exec_thread, this, true);
        resource_holder_list.insert(resource.reference_pointer);
    }
}

void Mutex::unlock()
{
    db<Synchronizer>(TRC) << "Mutex::unlock(this=" << this << ")" << endl;

    lock_for_releasing();
    if (_waiting.empty())
        _locked = false;
    else
        wakeup();
    unlock_for_releasing();
}

__END_SYS
