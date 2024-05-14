// EPOS Heap Utility Declarations

#ifndef __heap_h
#define __heap_h

#include <utility/debug.h>
#include <utility/list.h>
#include <utility/spin.h>

__BEGIN_UTIL

extern "C"
{
    void _lock_heap();
    void _unlock_heap();
}

// Heap
class Simple_Heap : private Grouping_List<char>
{
protected:
    static const bool typed = Traits<System>::multiheap;

public:
    using Grouping_List<char>::empty;
    using Grouping_List<char>::size;
    using Grouping_List<char>::grouped_size;

    Simple_Heap()
    {
        db<Init, Heaps>(TRC) << "Heap() => " << this << endl;
    }

    Simple_Heap(void *addr, unsigned long bytes)
    {
        db<Init, Heaps>(TRC) << "Heap(addr=" << addr << ",bytes=" << bytes << ") => " << this << endl;

        free(addr, bytes);
    }

    void *alloc(unsigned long bytes)
    {
        db<Heaps, Thread>(WRN) << "Heap::alloc(this=" << this << ",bytes=" << bytes;

        if (!bytes)
        {
            return 0;
        }

        if (!Traits<CPU>::unaligned_memory_access)
		{
            while ((bytes % sizeof(void *)))
			{
                ++bytes;
			}
		}

        if (typed) 
		{
            bytes += sizeof(void *); // add room for heap pointer
		}
        bytes += sizeof(long);       // add room for size
        if (bytes < sizeof(Element))
		{
            bytes = sizeof(Element);
		}

        Element *e = search_decrementing(bytes);
        if (!e)
        {


            out_of_memory(bytes);
            //leave_heap();
            return 0;
        }

        long *addr = reinterpret_cast<long *>(e->object() + e->size());

        if (typed) 
		{
			*addr++ = reinterpret_cast<long>(this);
		}
        *addr++ = bytes;

        db<Heaps>(TRC) << ") => " << reinterpret_cast<void *>(addr) << endl;

        //leave_heap();
        return addr;
    }

    void free(void *ptr, unsigned long bytes)
    {
        //enter_heap();

        db<Heaps>(TRC) << "Heap::free(this=" << this << ",ptr=" << ptr << ",bytes=" << bytes << ")" << endl;

        if (ptr && (bytes >= sizeof(Element)))
        {
            Element *e = new (ptr) Element(reinterpret_cast<char *>(ptr), bytes);
            Element *m1, *m2;
            insert_merging(e, &m1, &m2);
        }

        //leave_heap();
    }

    static void typed_free(void *ptr)
    {
        long *addr = reinterpret_cast<long *>(ptr);
        unsigned long bytes = *--addr;
        Simple_Heap *heap = reinterpret_cast<Simple_Heap *>(*--addr);
        heap->free(addr, bytes);
    }

    static void untyped_free(Simple_Heap *heap, void *ptr)
    {
        long *addr = reinterpret_cast<long *>(ptr);
        unsigned long bytes = *--addr;
        heap->free(addr, bytes);
    }

private:
    void out_of_memory(unsigned long bytes);

    static void enter_heap() { _lock_heap(); }
    static void leave_heap() { _unlock_heap(); }
};

// Wrapper for non-atomic heap
template<typename T, bool atomic>
class Heap_Wrapper: public T
{
public:
    Heap_Wrapper() {}
    Heap_Wrapper(void * addr, unsigned int bytes): T(addr, bytes) {}
};

template<typename T>
class Heap_Wrapper<T, true>: public T
{
public:
    Heap_Wrapper() {}
    Heap_Wrapper(void * addr, unsigned int bytes): T(addr, bytes) {}

    bool empty() {
		db<Thread>(WRN) << "locking on empty()\n" << endl;
        enter();
        bool tmp = T::empty();
        leave();
		db<Thread>(WRN) << "leaving lock on empty()\n" << endl;
        return tmp;
    }

    unsigned long size() {
		db<Thread>(WRN) << "locking on size()\n" << endl;
        enter();
        unsigned long tmp = T::size();
        leave();
		db<Thread>(WRN) << "leaving lock on size()\n" << endl;
        return tmp;
    }

    void * alloc(unsigned long bytes) {
		db<Thread>(WRN) << "locking on alloc()\n" << endl;
        enter();
        void * tmp = T::alloc(bytes);
        leave();
		db<Thread>(WRN) << "leaving lock on alloc()\n" << endl;
        return tmp;
    }

    void free(void * ptr) {
		db<Thread>(WRN) << "locking on free(ptr)\n" << endl;
        enter();
        T::free(ptr);
        leave();
		db<Thread>(WRN) << "leaving lock on free(ptr)\n" << endl;
    }

    void free(void * ptr, unsigned long bytes) {
		db<Thread>(WRN) << "locking on free(ptr, bytes)\n" << endl;
        enter();
        T::free(ptr, bytes);
        leave();
		db<Thread>(WRN) << "leaving lock on free(ptr, bytes)\n" << endl;
    }

private:
    void enter() { _lock_heap(); }
    void leave() { _unlock_heap(); }
};

// Heap
class Heap: public Heap_Wrapper<Simple_Heap, Traits<System>::multicore>
{
private:
    typedef Heap_Wrapper<Simple_Heap, Traits<System>::multicore> Base;

public:
    Heap() {}
    Heap(void * addr, unsigned long bytes): Base(addr, bytes) {}
};

__END_UTIL

#endif
