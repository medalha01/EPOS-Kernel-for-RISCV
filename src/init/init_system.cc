// EPOS System Initializer

#include <utility/random.h>
#include <machine.h>
#include <memory.h>
#include <system.h>
#include <process.h>

__BEGIN_SYS

class Init_System
{
private:
    static const unsigned int HEAP_SIZE = Traits<System>::HEAP_SIZE;

public:
    Init_System() {
        db<Init>(TRC) << "Init_System()" << endl;

        db<Init>(INF) << "Init:si=" << *System::info() << endl;

        db<Init>(INF) << "Initializing the architecture: " << endl;

        CPU::init();

        db<Init>(INF) << "Initializing system's heap: " << endl;

		// This barrier ensures that the bootstrap core has initialized the MMU,
		// which, for No_MMU systems and different MMUs in general, requires allocations
		// and/or freeing operations. Thus, it needs to be awaited by the other cores.
		db<Thread>(WRN) << "@@@INITSYSTEM antes do barrier do começo " << endl;
		CPU::smp_barrier();
		db<Thread>(WRN) << "@@@INITSYSTEM DEPOIS do barrier do começo " << endl;

		if (CPU::is_bootstrap()) 
		{
			if(Traits<System>::multiheap) 
			{
				System::_heap_segment = new (&System::_preheap[0]) Segment(HEAP_SIZE, Segment::Flags::SYSD);
				char * heap;

				if(Memory_Map::SYS_HEAP == Traits<Machine>::NOT_USED)
					heap = Address_Space(MMU::current()).attach(System::_heap_segment);
				else
					heap = Address_Space(MMU::current()).attach(System::_heap_segment, Memory_Map::SYS_HEAP);
				if(!heap)
					db<Init>(ERR) << "Failed to initialize the system's heap!" << endl;

				System::_heap = new (&System::_preheap[sizeof(Segment)]) Heap(heap, System::_heap_segment->size());
			} 
			else
			{
				System::_heap = new (&System::_preheap[0]) Heap(MMU::alloc(MMU::pages(HEAP_SIZE)), HEAP_SIZE);
			}
		}

		// This one is rather simple - the bootstrap core is initializing the heap, 
		// and, as such, it is paramount that the other cores await for its completion.
		db<Thread>(WRN) << "@@@INITSYSTEM antes do barrier do meio " << endl;
		CPU::smp_barrier();
		db<Thread>(WRN) << "@@@INITSYSTEM DEPOIS do barrier do meio " << endl;

        db<Init>(INF) << "Initializing the machine: " << endl;
        Machine::init();

        db<Init>(INF) << "Initializing system abstractions: " << endl;
        System::init();

        // Randomize the Random Numbers Generator's seed
        if(CPU::is_bootstrap() && Traits<Random>::enabled) {
            db<Init>(INF) << "Randomizing the Random Numbers Generator's seed." << endl;

            if(Traits<TSC>::enabled)
			{
                Random::seed(TSC::time_stamp());
			}

            if(!Traits<TSC>::enabled)
			{
                db<Init>(INF) << "Due to lack of entropy, "
					<< "Random is a pseudo random numbers generator!"
					<< endl;
			}
        }

		// Waits for the bootstrap core to properly initialize the static _seed
		// inside the Random class. If other cores try to utilize random(), it
		// is possible that there would be some issues without this barrier.
		db<Thread>(WRN) << "@@@INITSYSTEM antes ---final " << endl;
		CPU::smp_barrier();
		db<Thread>(WRN) << "@@@INITSYSTEM DEPOIS ---final" << endl;

        // Initialization continues at init_end
    }
};

// Global object "init_system" must be constructed first.
Init_System init_system;

__END_SYS
