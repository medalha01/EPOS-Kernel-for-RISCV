// EPOS Scheduler Test Program

#include <machine/display.h>
#include <time.h>
#include <synchronizer.h>
#include <process.h>

using namespace EPOS;

Mutex hello_lock;

Thread *hellos[Traits<Build>::CPUS];

OStream cout;

int hello();

int main()
{

    for (UInt i = 0; i < Traits<Build>::CPUS; i++)
        hellos[i] = new Thread(hello);

    cout << "Hellos are alive and buggy!" << endl;

    cout << "The end!" << endl;

    return 0;
}

int hello()
{

    hello_lock.lock();
    cout
        << "Hello world!" << "\n"
        << "My ID is: " << CPU::id() << endl;
    hello_lock.unlock();

    CPU::smp_barrier();

    return 1;
}
