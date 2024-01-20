

#include <cstddef>
#include <cstdint>
#include "TimingHelpers.h"
#include "Histogram.h"

// Defined in DuffLoopHelpers.cpp
void computation(int &data1, int &data2);
void computation(int &data1, int &data2)
{
    data1 += data2++;
}

void execute_loop_basic(int &data1, int &data2, const size_t loop_size)
{
    for (int i = 0; i < loop_size; ++i)
    {
        computation(data1, data2);
    }
}

static void testBasicLoop(uint64_t numloops)
{
    Histogram<64> hist(1, 1E6);
    timeit(hist, numloops, []()
           {
               int data1 = 33;
               int data2 = 0;
               execute_loop_basic(data1, data2, 10000);
               return 1;
           });
    std::cout << "Basic loop:" << '\n';
    hist.print(std::cout);
    std::cout << '\n';
}

void execute_loop_duff(int &data1, int &data2, const size_t loop_size)
{
    size_t i = 0;
    const size_t s = (loop_size + 3) / 4;
    switch (loop_size % 4)
    {
        do
        {
        case 0:
            computation(data1, data2);
        case 3:
            computation(data1, data2);
        case 2:
            computation(data1, data2);
        case 1:
            computation(data1, data2);
            ++i;
        } while (i < s);
    }
}

static void testDuffsDevice(uint64_t numloops)
{
    Histogram<64> hist(1, 1E6);
    timeit(hist, numloops, []()
           {
               int data1 = 33;
               int data2 = 0;
               execute_loop_duff(data1, data2, 10000);
               return 1;
           });
    std::cout << "Duffs device:" << '\n';
    hist.print(std::cout);
    std::cout << '\n';
}

int main()
{
    uint64_t nloops = 100000;
    testBasicLoop(nloops);
    testDuffsDevice(nloops);
}