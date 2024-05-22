// EPOS Heap Utility Declarations

#ifndef __heap_h
#define __heap_h

#include <utility/debug.h>
#include <utility/list.h>
#include <utility/spin.h>

__BEGIN_UTIL

// Heap
class Heap: private Grouping_List<char>
{
protected:
    static const bool typed = Traits<System>::multiheap;

public:
	typedef Grouping_List<char> Base;

    using Base::empty;
    using Base::size;
    using Base::grouped_size;

    Heap() 
	{
        db<Init, Heaps>(TRC) << "Heap() => " << this << endl;
    }

    Heap(void * addr, unsigned long bytes) 
	{
        db<Init, Heaps>(TRC) << "Heap(addr=" << addr << ",bytes=" << bytes << ") => " << this << endl;

        free(addr, bytes);
    }

	unsigned long size() 
	{
		lock();
		auto temp = Base::size();
		unlock();
		return temp;
	}

	void * alloc(unsigned long bytes) 
	{
		lock();
		auto temp = _helper_alloc(bytes);
		unlock();
		return temp;
	}

	void free(void * ptr, unsigned long bytes) 
	{
		lock();
		_helper_free(ptr, bytes);
		unlock();
	}

    static void typed_free(void * ptr) 
	{
        long * addr = reinterpret_cast<long *>(ptr);
        unsigned long bytes = *--addr;
        Heap * heap = reinterpret_cast<Heap *>(*--addr);
        heap->free(addr, bytes);
    }

    static void untyped_free(Heap * heap, void * ptr) 
	{
        long * addr = reinterpret_cast<long *>(ptr);
        unsigned long bytes = *--addr;
        heap->free(addr, bytes);
    }

private:
	//Spin _spin;
	Simple_Spin _spin;

	void lock()   { _spin.acquire(); }
	void unlock() { _spin.release(); }

    void out_of_memory(unsigned long bytes);

    //void * alloc(unsigned long bytes) {
    void * _helper_alloc(unsigned long bytes)
	{
        db<Heaps>(TRC) << "Heap::alloc(this=" << this << ",bytes=" << bytes;

        if(!bytes)
            return 0;

        if(!Traits<CPU>::unaligned_memory_access)
            while((bytes % sizeof(void *)))
                ++bytes;

        if(typed)
            bytes += sizeof(void *);  // add room for heap pointer
        bytes += sizeof(long);        // add room for size
        if(bytes < sizeof(Element))
            bytes = sizeof(Element);

        Element * e = search_decrementing(bytes);
        if(!e) 
		{
            out_of_memory(bytes);
            return 0;
        }

        long * addr = reinterpret_cast<long *>(e->object() + e->size());

        if(typed)
            *addr++ = reinterpret_cast<long>(this);
        *addr++ = bytes;

        db<Heaps>(TRC) << ") => " << reinterpret_cast<void *>(addr) << endl;

        return addr;
    }

    void _helper_free(void * ptr, unsigned long bytes) 
	{
        db<Heaps>(TRC) << "Heap::free(this=" << this << ",ptr=" << ptr << ",bytes=" << bytes << ")" << endl;

        if(ptr && (bytes >= sizeof(Element))) 
		{
            Element * e = new (ptr) Element(reinterpret_cast<char *>(ptr), bytes);
            Element * m1, * m2;
            insert_merging(e, &m1, &m2);
        }
    }
};

__END_UTIL

#endif
