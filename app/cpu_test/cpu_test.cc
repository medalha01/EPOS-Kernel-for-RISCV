// EPOS Generic CPU Test Program

#include <architecture/cpu.h>

using namespace EPOS;

OStream cout;

volatile bool lock = false;     // solução preventiva, pois causa desalinhamento de memória se estiver dentro da main
int main()
{
    cout << "CPU test" << endl;

    CPU cpu;

    {
        cout << "CPU::tsl(l=false)\t=> ";

        if(cpu.tsl(lock))
            cout << "failed [1]!" << endl;
        else
            if(cpu.tsl(lock))
                cout << "passed!" << endl;
            else
                cout << "failed [2]!" << endl;
    }
    {
        cout << "CPU::finc(n=100)\t=> ";

        volatile int number = 100;
        volatile int tmp;
        if((tmp = cpu.finc(number)) != 100)
            cout << "failed (n=" << tmp << ", should be 100)!" << endl;
        else
            if((tmp = cpu.finc(number)) != 101)
                cout << "failed (n=" << tmp << ", should be 101)!" << endl;
            else
                cout << "passed!" << endl;
    }
    {
        cout << "CPU::fdec(n=100)\t=> ";

        volatile int number = 100;
        volatile int tmp;
        if((tmp = cpu.fdec(number)) != 100)
            cout << "failed (n=" << tmp << ", should be 100)!" << endl;
        else
            if((tmp = cpu.fdec(number)) != 99)
                cout << "failed (n=" << tmp << ", should be 99)!" << endl;
            else
                cout << "passed!" << endl;
    }
    {
        cout << "CPU::cas(n=100)\t\t=> ";

        volatile int number = 100;
        volatile int compare = number;
        volatile int replacement = number - 1;
        volatile int tmp;
        if((tmp = cpu.cas(number, compare, replacement)) != compare)
            cout << "failed [1] (n=" << tmp << ", should be " << compare << ")!" << endl;
        else
            if((tmp = cpu.cas(number, compare, replacement)) != replacement)
                cout << "failed [2] (n=" << tmp << ", should be " << replacement << ")!" << endl;
            else
                cout << "passed!" << endl;
    }

    cout << "I'm done, bye!" << endl;

    return 0;
}
