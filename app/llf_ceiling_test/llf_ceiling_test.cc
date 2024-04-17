// EPOS Periodic Thread Component Test Program

#include <time.h>
#include <real-time.h>

using namespace EPOS;

long max(unsigned int a, unsigned int b, unsigned int c) { return ((a >= b) && (a >= c)) ? a : ((b >= a) && (b >= c) ? b : c); }

// Global configuration constants
const unsigned int iterations = 5;
const unsigned int periods[] = {2000, 2200, 2400};   // ms for threads A, B, C
const unsigned int wcets[] = {500, 500, 500};        // ms for threads A, B, C
const unsigned int deadlines[] = {2000, 2200, 2400}; // ms for threads A, B, C
const unsigned int period_w = 100;                   // ms for watcher thread
const unsigned int wcet_w = 10;                      // ms for watcher thread
const unsigned int deadline_w = 100;                 // ms for watcher thread

// Function prototypes
int func_a();
int func_b();
int func_c();
int func_w();

Chronometer chrono;
OStream cout;
Periodic_Thread *threads[4];
int matrices[5][12][12]; // Five matrices A-E, to simplify passing to functions

// Helper functions
unsigned int my_rand(unsigned int seed);
void generate_matrix(int index, unsigned int seed);
void matrix_multiply(int index_a, int index_b, int index_c, int index_d, int index_e);
void exec(char c, unsigned int time = 0);

int main()
{
    cout << "Periodic Thread Component Test" << endl;

    threads[0] = new Periodic_Thread(RTConf(periods[0] * 1000, deadlines[0] * 1000, wcets[0] * 1000, 0, iterations), &func_a);
    threads[1] = new Periodic_Thread(RTConf(periods[1] * 1000, deadlines[1] * 1000, wcets[1] * 1000, 0, iterations), &func_b);
    threads[2] = new Periodic_Thread(RTConf(periods[2] * 1000, deadlines[2] * 1000, wcets[2] * 1000, 0, iterations), &func_c);
    threads[3] = new Periodic_Thread(RTConf(period_w * 1000, deadline_w * 1000, wcet_w * 1000, 0, 200), &func_w);

    exec('M');
    chrono.start();

    for (int i = 0; i < 3; ++i)
    {
        threads[i]->join();
    }
    threads[3]->join();
    chrono.stop();
    exec('M');
    cout << "\nThe estimated time to run the test was "
         << max(periods[0], periods[1], periods[2]) * iterations
         << " ms. The measured time was " << chrono.read() / 1000 << " ms!" << endl;

    cout << "I'm also done, bye!" << endl;

    cout << "All threads have completed." << endl;
    return 0;
}

int func_a()
{
    generate_matrix(0, 123);
    generate_matrix(1, 123);
    generate_matrix(2, 123);
    do
    {
        cout << "A running - Priority: " << threads[0]->priority() << endl;
        matrix_multiply(0, 1, 2, 3, 4);
        cout << "A Finished - Priority: " << threads[0]->priority() << endl;
    } while (Periodic_Thread::wait_next());
    return 'A';
}

int func_b()
{
    generate_matrix(0, 123);
    generate_matrix(1, 123);
    generate_matrix(2, 123);
    do
    {
        cout << "B running - Priority: " << threads[1]->priority() << endl;
        matrix_multiply(0, 1, 2, 3, 4);
        cout << "B finished - Priority: " << threads[1]->priority() << endl;
    } while (Periodic_Thread::wait_next());
    return 'B';
}

int func_c()
{
    generate_matrix(0, 123);
    generate_matrix(1, 123);
    generate_matrix(2, 123);
    do
    {
        cout << "C running - Priority: " << threads[2]->priority() << endl;
        matrix_multiply(0, 1, 2, 3, 4);
        cout << "C Finished - Priority: " << threads[2]->priority() << endl;

    } while (Periodic_Thread::wait_next());
    return 'C';
}

int func_w()
{
    do
    {
        cout << "Watcher active" << endl;
        exec('W', wcet_w);
    } while (Periodic_Thread::wait_next());
    return 'W';
}

unsigned int my_rand(unsigned int seed)
{
    unsigned int rand_state = seed;
    rand_state = (1103515245 * rand_state + 12345) % 21474836418;
    return rand_state;
}

void generate_matrix(int index, unsigned int seed)
{
    cout << "Starting Matrix Generation:" << chrono.read() / 1000 << endl;
    for (int i = 0; i < 12; i++)
    {
        for (int j = 0; j < 12; j++)
        {
            matrices[index][i][j] = my_rand(seed + i + j) % 100;
        }
    }
    cout << "Ending Matrix Generation" << chrono.read() / 1000 << endl;
}

void matrix_multiply(int index_a, int index_b, int index_c, int index_d, int index_e)
{
    cout << "Starting Matrix Multiplication" << chrono.read() / 1000 << endl;

    int n = 12;
    // Initialize matrices D and E
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            matrices[index_d][i][j] = 0;
            matrices[index_e][i][j] = 0;
        }
    }

    // Matrix multiplication logic
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            for (int k = 0; k < n; k++)
            {
                matrices[index_d][i][j] += matrices[index_a][i][k] * matrices[index_b][k][j];
                matrices[index_e][i][j] += matrices[index_d][i][k] * matrices[index_c][k][j];
                cout << "Matrix"
                     << "I" << i << "J" << j << "Value" << matrices[index_e][i][j] << endl;
            }
        }
    }

    cout << "Ending Matrix Multiplication " << chrono.read() / 1000 << endl;
}

void exec(char c, unsigned int time)
{
    Microsecond elapsed = chrono.read() / 1000;
    cout << elapsed << "\t" << c << "\t Priority levels: [A=" << threads[0]->priority() << ", B=" << threads[1]->priority() << ", C=" << threads[2]->priority() << "]" << endl;

    if (time)
    {
        for (Microsecond end = elapsed + time, last = end; end > elapsed; elapsed = chrono.read() / 1000)
            if (last != elapsed)
            {
                cout << elapsed << "\t" << c << "\t Priority levels: [A=" << threads[0]->priority() << ", B=" << threads[1]->priority() << ", C=" << threads[2]->priority() << "]" << endl;

                last = elapsed;
            }
    }
}
