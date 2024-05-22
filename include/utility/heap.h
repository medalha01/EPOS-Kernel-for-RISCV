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
    using Grouping_List<char>::empty;
    using Grouping_List<char>::size;
    using Grouping_List<char>::grouped_size;

    Heap() 
	{
        db<Init, Heaps>(TRC) << "Heap() => " << this << endl;
    }

    Heap(void * addr, unsigned long bytes) 
	{
        db<Init, Heaps>(TRC) << "Heap(addr=" << addr << ",bytes=" << bytes << ") => " << this << endl;

		db<Thread>(WRN) << "@@@TEST tem sti? sti = " << CPU::int_enabled_p(5) << endl;
		db<Thread>(WRN) << "@@@TEST tem mti? mti = " << CPU::int_enabled_p(7) << endl;

		db<Thread>(WRN) << "@@@TEST outros = " << CPU::int_enabled_p(3) << endl;
		db<Thread>(WRN) << "@@@TEST outros = " << CPU::int_enabled_p(4) << endl;
		db<Thread>(WRN) << "@@@TEST outros = " << CPU::int_enabled_p(6) << endl;
		db<Thread>(WRN) << "@@@TEST outros = " << CPU::int_enabled_p(8) << endl;
		db<Thread>(WRN) << "@@@TEST outros = " << CPU::int_enabled_p(9) << endl;

		db<Thread>(WRN) << "@@@TEST mip em geral = " << CPU::mip() << endl;
		db<Thread>(WRN) << "@@@TEST sip em geral = " << CPU::sip() << endl;

        free(addr, bytes);
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
		db<Thread>(WRN) << "@@@SPIN: antes do lock()" << endl;

		lock();

		db<Thread>(WRN) << "@@@SPIN: depois do lock()" << endl;

		_helper_free(ptr, bytes);

		db<Thread>(WRN) << "@@@SPIN: antes do unlock()" << endl;

		unlock();

		db<Thread>(WRN) << "@@@SPIN: depois do unlock()" << endl;
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
