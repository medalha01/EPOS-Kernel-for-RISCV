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
class Heap : private Grouping_List<char>
{
protected:
    static const bool typed = Traits<System>::multiheap;

public:
    using Grouping_List<char>::empty;
    using Grouping_List<char>::size;
    using Grouping_List<char>::grouped_size;

    Heap()
    {
        db<Init, Heaps>(TRC) << "Heap() => " << this << endl;
    }

    Heap(void *addr, unsigned long bytes)
    {
        db<Init, Heaps>(TRC) << "Heap(addr=" << addr << ",bytes=" << bytes << ") => " << this << endl;

        free(addr, bytes);
    }

    void *alloc(unsigned long bytes)
    {
        enter_heap();
        db<Heaps>(TRC) << "Heap::alloc(this=" << this << ",bytes=" << bytes;

        if (!bytes)
        {
            leave_heap();
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
        bytes += sizeof(long); // add room for size
        if (bytes < sizeof(Element))
        {
            bytes = sizeof(Element);
        }

        Element *e = search_decrementing(bytes);
        if (!e)
        {

            out_of_memory(bytes);
            leave_heap();
            return 0;
        }

        long *addr = reinterpret_cast<long *>(e->object() + e->size());

        if (typed)
        {
            *addr++ = reinterpret_cast<long>(this);
        }
        *addr++ = bytes;

        db<Heaps>(TRC) << ") => " << reinterpret_cast<void *>(addr) << endl;

        leave_heap();
        return addr;
    }

    void free(void *ptr, unsigned long bytes)
    {
        enter_heap();

        db<Heaps>(TRC) << "Heap::free(this=" << this << ",ptr=" << ptr << ",bytes=" << bytes << ")" << endl;

        if (ptr && (bytes >= sizeof(Element)))
        {
            Element *e = new (ptr) Element(reinterpret_cast<char *>(ptr), bytes);
            Element *m1, *m2;
            insert_merging(e, &m1, &m2);
        }

        leave_heap();
    }

    void non_lock_free(void *ptr, unsigned long bytes)
    {

        db<Heaps>(TRC) << "Heap::free(this=" << this << ",ptr=" << ptr << ",bytes=" << bytes << ")" << endl;

        if (ptr && (bytes >= sizeof(Element)))
        {
            Element *e = new (ptr) Element(reinterpret_cast<char *>(ptr), bytes);
            Element *m1, *m2;
            insert_merging(e, &m1, &m2);
        }
    }

    static void typed_free(void *ptr)
    {
        long *addr = reinterpret_cast<long *>(ptr);
        unsigned long bytes = *--addr;
        Heap *heap = reinterpret_cast<Heap *>(*--addr);

        heap->non_lock_free(addr, bytes);
    }

    static void untyped_free(Heap *heap, void *ptr)
    {
        enter_heap();
        long *addr = reinterpret_cast<long *>(ptr);
        unsigned long bytes = *--addr;
        heap->non_lock_free(addr, bytes);
        leave_heap();
    }

private:
    void out_of_memory(unsigned long bytes);

    static void enter_heap() { _lock_heap(); }
    static void leave_heap() { _unlock_heap(); }
};

__END_UTIL

#endif
