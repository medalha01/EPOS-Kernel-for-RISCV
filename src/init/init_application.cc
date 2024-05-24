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
    Init_Application() {
        db<Init>(TRC) << "Init_Application()" << endl;
		//db<Thread>(WRN) << "@@@INIT APP\n\n\n\n" << endl;

        // Initialize Application's heap
        db<Init>(INF) << "Initializing application's heap: ";

		// TODO: @arthur possivelmente tirar esse barrier. vide comentário abaixo.
		db<Thread>(WRN) << "@@@INITAPP antes --- start" << endl;
		CPU::smp_barrier();
		db<Thread>(WRN) << "@@@INITAPP DEPOIS -- start" << endl;

		// TODO: @arthur ver onde que o Init_Application é chamado na hierarquia de calls.
		if (CPU::is_bootstrap())
		{
			if(Traits<System>::multiheap) 
			{ 
				// heap in data segment arranged by SETUP
				db<Init>(INF) << endl;

				// ld is eliminating the data segment in some compilations,
				// particularly for RISC-V, and placing _end in the code segment
				char * heap = (MMU::align_page(&_end) >= CPU::Log_Addr(Memory_Map::APP_DATA)) 
					? MMU::align_page(&_end)
					: CPU::Log_Addr(Memory_Map::APP_DATA); 

				Application::_heap = new (&Application::_preheap[0]) Heap(heap, HEAP_SIZE);
			} 
			else
			{
				db<Init>(INF) << "adding all free memory to the unified system's heap!" << endl;

				// TODO: @arthur verificar se isso aqui está correto para múltiplos cores
				for(unsigned int frames = MMU::allocable(); frames; frames = MMU::allocable())
				{
					System::_heap->free(MMU::alloc(frames), frames * sizeof(MMU::Page));
				}
			}
		}

		// Guarantees that the previous heap operations were initialized successfully. 
		// Otherwise, different cores might be trying to perform allocations, which would
		// result in memory errors, as either there is no heap in the system just yet,
		// or the heap freeing operation that unifies it into the system's heap has not 
		// yet been completed by the bootstrap core.
		db<Thread>(WRN) << "@@@INITAPP antes --- final" << endl;
		CPU::smp_barrier();
		db<Thread>(WRN) << "@@@INITAPP DEPOIS -- final" << endl;
    }
};

// Global object "init_application"  must be linked to the application (not to the system) and there constructed at first.
Init_Application init_application;

__END_SYS
