// EPOS Periodic Thread Component Test Program

#include <time.h>
#include <real-time.h>
// #include <ceiling_utils.h>  // Make sure this includes all matrix operations and necessary headers

using namespace EPOS;
/*
const unsigned int iterations = 100;
const unsigned int period_a = 100;   // ms
const unsigned int period_b = 80;    // ms
const unsigned int period_c = 60;    // ms
const unsigned int wcet_a = 50;      // ms
const unsigned int wcet_b = 20;      // ms
const unsigned int wcet_c = 10;      // ms
const unsigned int deadline_a = 100; // ms
const unsigned int deadline_b = 80;  // ms
const unsigned int deadline_c = 60;  // ms*/

const unsigned int iterations = 10;
const unsigned int period_a = 600;    // ms
const unsigned int period_b = 800;    // ms
const unsigned int period_c = 1000;   // ms
const unsigned int wcet_a = 200;      // ms
const unsigned int wcet_b = 200;      // ms
const unsigned int wcet_c = 300;      // ms
const unsigned int deadline_a = 600;  // ms
const unsigned int deadline_b = 800;  // ms
const unsigned int deadline_c = 1000; // ms

int func_a();
int func_b();
int func_c();
long max(unsigned int a, unsigned int b, unsigned int c) { return ((a >= b) && (a >= c)) ? a : ((b >= a) && (b >= c) ? b : c); }

Chronometer chrono;
OStream cout;
Periodic_Thread *thread_a;
Periodic_Thread *thread_b;
Periodic_Thread *thread_c;

int matrix_a[512][512], matrix_b[512][512], matrix_c[512][512], matrix_d[512][512], matrix_e[512][512];



int N = 512;
int A = 1103515245;
int C = 12345;
int M = 2147483648;

unsigned int rand_state = 123; // Initial seed
// OStream cout;

// Pseudo-random number generator using the Linear Congruential Generator method
unsigned int my_rand(int seed) {
    rand_state = (A * seed + C) % M;
    return rand_state;
}

void generate_matrix(int matrix[512][512], int seed) {
    int i, j;

    // Populate the matrix with random numbers
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            matrix[i][j] = my_rand(seed) % 100; // Random numbers between 0 and 99
        }
    }
}

void matrix_multiply(int n, int A[512][512], int B[512][512], int C[512][512], int D[512][512], int E[512][512]) {
    int i, j, k;

    // Initialize the result matrix to zero
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            D[i][j] = 0;
            E[i][j] = 0;
        }
    }

    // Multiply matrices
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            for (k = 0; k < n; k++) {
                D[i][j] += A[i][k] * B[k][j];
            }
        }
    }

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            for (k = 0; k < n; k++) {
                E[i][j] += D[i][k] * C[k][j];
            }
        }
    }
}

inline void exec(char c, unsigned int time = 0) // in miliseconds
{
    // Delay was not used here to prevent scheduling interference due to blocking
    Microsecond elapsed = chrono.read() / 1000;

    cout << "\n"
         << elapsed << "\t" << c
         << "\t[p(A)=" << thread_a->priority()
         << ", p(B)=" << thread_b->priority()
         << ", p(C)=" << thread_c->priority() << "]";

    if (time)
    {
        for (Microsecond end = elapsed + time, last = end; end > elapsed; elapsed = chrono.read() / 1000)
            if (last != elapsed)
            {
                cout << "\n"
                     << elapsed << "\t" << c
                     << "\t[p(A)=" << thread_a->priority()
                     << ", p(B)=" << thread_b->priority()
                     << ", p(C)=" << thread_c->priority() << "]";
                last = elapsed;
            }
    }
}

int main()
{
    cout << "Periodic Thread Component Test" << endl;

    cout << "\nThis test consists in creating three periodic threads as follows:" << endl;
    cout << "- Every " << period_a << "ms, thread A execs \"a\", waits for " << wcet_a << "ms and then execs another \"a\";" << endl;
    cout << "- Every " << period_b << "ms, thread B execs \"b\", waits for " << wcet_b << "ms and then execs another \"b\";" << endl;
    cout << "- Every " << period_c << "ms, thread C execs \"c\", waits for " << wcet_c << "ms and then execs another \"c\";" << endl;

    cout << "Threads will now be created and I'll wait for them to finish..." << endl;

    // p,d,c,act,t
    thread_a = new Periodic_Thread(RTConf(period_a * 1000, deadline_a * 1000, wcet_a * 1000, 0, iterations), &func_a);
    thread_b = new Periodic_Thread(RTConf(period_b * 1000, deadline_b * 1000, wcet_b * 1000, 0, iterations), &func_b);
    thread_c = new Periodic_Thread(RTConf(period_c * 1000, deadline_c * 1000, wcet_c * 1000, 0, iterations), &func_c);

    exec('M');

    chrono.start();

    int status_a = thread_a->join();
    int status_b = thread_b->join();
    int status_c = thread_c->join();

    chrono.stop();

    exec('M');

    cout << "\n... done!" << endl;
    cout << "\n\nThread A exited with status \"" << char(status_a)
         << "\", thread B exited with status \"" << char(status_b)
         << "\" and thread C exited with status \"" << char(status_c) << "." << endl;

    cout << "\nThe estimated time to run the test was "
         << max(period_a, period_b, period_c) * iterations
         << " ms. The measured time was " << chrono.read() / 1000 << " ms!" << endl;

    cout << "I'm also done, bye!" << endl;

    return 0;
}

int func_a() {
    exec('A'); // Initial logging

    // Define and initialize matrices
    generate_matrix(matrix_a, 123);
    generate_matrix(matrix_b, 123);
    generate_matrix(matrix_c, 123);

    do {
        cout << "\nStart of A - Priority: " << thread_a->priority();
        exec('a', wcet_a); // Simulating work with priority logging
        matrix_multiply(512, matrix_a, matrix_b, matrix_c, matrix_d, matrix_e);
        cout << "\nEnd of A - Priority: " << thread_a->priority() << "\n";
    } while (Periodic_Thread::wait_next());

    exec('A');

    return 'A';
}

int func_b() {
    exec('B'); // Initial logging

    // Define and initialize matrices
    generate_matrix(matrix_a, 123);
    generate_matrix(matrix_b, 123);
    generate_matrix(matrix_c, 123);

    do {
        cout << "\nStart of B - Priority: " << thread_b->priority();
        exec('b', wcet_b); // Simulating work with priority logging
        matrix_multiply(512, matrix_a, matrix_b, matrix_c, matrix_d, matrix_e);
        cout << "\nEnd of B - Priority: " << thread_b->priority() << "\n";
    } while (Periodic_Thread::wait_next());

    exec('B');

    return 'B';
}

int func_c() {
    exec('C'); // Initial logging

    // Define and initialize matrices
    generate_matrix(matrix_a, 123);
    generate_matrix(matrix_b, 123);
    generate_matrix(matrix_c, 123);

    do {
        cout << "\nStart of C - Priority: " << thread_c->priority();
        exec('c', wcet_c); // Simulating work with priority logging
        matrix_multiply(512, matrix_a, matrix_b, matrix_c, matrix_d, matrix_e);
        cout << "\nEnd of C - Priority: " << thread_c->priority() << "\n";
    } while (Periodic_Thread::wait_next());

    exec('C');

    return 'C';
}
