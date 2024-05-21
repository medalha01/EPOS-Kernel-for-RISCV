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
    Init_System()
    {
        db<Init>(TRC) << "Init_System()" << endl;
        CPU::smp_barrier();

        db<Init>(INF) << "Init: si = " << *System::info() << endl;

        if (CPU::is_bootstrap())
        {
            initialize_system_heap();
        }
        else
        {
            machine_init_non_bootstrap();
        }

        db<Init>(INF) << "Initializing system abstractions: " << endl;
        System::init();

        randomize_seed();

        // TODO: fix this
        // waits until the bootstrap CPU signalizes "machine ready"
        CPU::smp_barrier();

        // Initialization continues at init_end
    }

private:
    void initialize_system_heap()
    {
        db<Init>(INF) << "Initializing the architecture: " << endl;
        CPU::init();

        db<Init>(INF) << "Initializing system's heap: " << endl;
        if (Traits<System>::multiheap)
        {
            System::_heap_segment = new (&System::_preheap[0]) Segment(HEAP_SIZE, Segment::Flags::SYSD);
            char *heap = (Memory_Map::SYS_HEAP == Traits<Machine>::NOT_USED) ? Address_Space(MMU::current()).attach(System::_heap_segment) : Address_Space(MMU::current()).attach(System::_heap_segment, Memory_Map::SYS_HEAP);

            if (!heap)
            {
                db<Init>(ERR) << "Failed to initialize the system's heap!" << endl;
            }

            System::_heap = new (&System::_preheap[sizeof(Segment)]) Heap(heap, System::_heap_segment->size());
        }
        else
        {
            System::_heap = new (&System::_preheap[0]) Heap(MMU::alloc(MMU::pages(HEAP_SIZE)), HEAP_SIZE);
        }

        db<Init>(INF) << "Initializing the machine: " << endl;
        Machine::init();
        CPU::smp_barrier();
    }

    void machine_init_non_bootstrap()
    {
        CPU::smp_barrier();
        CPU::init();
        Timer::init();
    }

    void randomize_seed()
    {
        if (Traits<Random>::enabled)
        {
            db<Init>(INF) << "Randomizing the Random Numbers Generator's seed." << endl;

            if (Traits<TSC>::enabled)
            {
                Random::seed(TSC::time_stamp());
            }
            else
            {
                db<Init>(INF) << "Due to lack of entropy, Random is a pseudo random numbers generator!" << endl;
            }
        }
    }
};

// Global object "init_system" must be constructed first.
Init_System init_system;

__END_SYS
