// EPOS Application Initializer

#include <architecture.h>
#include <utility/heap.h>
#include <machine.h>
#include <system.h>

extern "C" char _end; // defined by GCC

__BEGIN_SYS

class Init_Application
{
private:
    static const unsigned int HEAP_SIZE = Traits<Application>::HEAP_SIZE;
    static const unsigned int STACK_SIZE = Traits<Application>::STACK_SIZE;

public:
    Init_Application()
    {
		if (CPU::is_bootstrap()) 
		{
			if (Traits<System>::multiheap)
			{ 
				// heap in data segment arranged by SETUP
				db<Init>(INF) << endl;
				//
				// ld is eliminating the data segment in some compilations,
				// particularly for RISC-V, and placing _end in the code segment
				char *heap = (MMU::align_page(&_end) >= CPU::Log_Addr(Memory_Map::APP_DATA)) 
					? MMU::align_page(&_end)
					: CPU::Log_Addr(Memory_Map::APP_DATA); 

				if (Traits<Build>::SMOD != Traits<Build>::KERNEL)
				{
					// if not a kernel, then use the stack allocated by SETUP,
					// otherwise make that part of the heap
					heap += MMU::align_page(Traits<Application>::STACK_SIZE);
				}

				Application::_heap = new (&Application::_preheap[0]) Heap(heap, HEAP_SIZE);
			}
			else
			{
				db<Init>(INF) << "adding all free memory to the unified system's heap!" << endl;
				for (unsigned int frames = MMU::allocable(); frames; frames = MMU::allocable())
				{
					System::_heap->free(MMU::alloc(frames), frames * sizeof(MMU::Page));
				}
			}
		}

		// Ensures all those heap operations were concluded successfully before moving on
		// - potential undefined behaviour would be lurking had this not been here.
        CPU::smp_barrier(); 
    }
};

// Global object "init_application"  must be linked to the application (not to the system) and there constructed at first.
Init_Application init_application;

__END_SYS
