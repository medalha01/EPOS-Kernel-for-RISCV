#include <utility/ostream.h>
#include <synchronizer.h>

using namespace EPOS;

OStream cout;

Mutex ao;
int main()
{
    ao.lock();
    cout
        << "Hello world!" << endl;
    ao.unlock();

    cout
        << "Peida não" << endl;
    return 0;
}
