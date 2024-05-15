// EPOS Scheduler Test Program

#include <machine/display.h>
#include <time.h>
#include <synchronizer.h>
#include <process.h>

using namespace EPOS;

Thread *allocers[Traits<Build>::CPUS];

OStream cout;

class A
{
public:
    int J = 80;
    int T = 1;
    int run()
    {
        for (; J > 0; J -= T)
        {
            for (int i = 0; i < 79; i++)
            {
                if ((i % 7 == 0) && (J % 7 == 0))
                {
                    cout << "My id is:  " << CPU::id() << endl;
                }
            }
        }

        return 'A';
    }
};

int alloc();

int main()
{

    for (UInt i = 0; i < Traits<Build>::CPUS; i++)
        allocers[i] = new Thread(alloc);

    cout << "Allocers are allocing" << endl;

    cout << "The end!" << endl;

    return 0;
}

int alloc()
{
    CPU::smp_barrier();
    int iterator = 0;
    while (iterator < 5)
    {
        A *a = new A;
        cout << a->J << a->T << endl;
        a->run();
        iterator++;
    }

    cout << "I stopped working:" << "  " << CPU::id();
    CPU::smp_barrier();

    return 1;
}
