#include <utility/ostream.h>
#include <synchronizer.h>

using namespace EPOS;

OStream cout;

Mutex ao;

int main()
{
    ao.lock();
    cout << "Hello world!" << endl;
    Machine.delay(1000000);
    ao.unlock();

    return 0;
}
