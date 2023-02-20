#include <cstdint>
#include <vector>
#include <iostream>

/** Just a nice encapsulation of the perf kernel's facilities. This is aimed
    at a simple self-monitoring, not meant as a generic wrapper. Notice this
    is not very performance-oriented. For cases with multiple sensors it is
    better to use the memory mapped interface which we dont provide here.
*/
class PerfCounter
{
public:
    /** Constructor that also initializes the underlying file */
    PerfCounter(unsigned int, long long);

    /** Constructor does not initialize */
    PerfCounter();

    /** Destructor */
    ~PerfCounter();

    /** Initializes the underlying file */
    bool init(unsigned type, long long event, int group = -1);

    /** Returns the underlying file ID - just used for grouping purposes */
    int fid();

    /** Enables the internal event capturing */
    bool start();

    /** Disables the internal event capturing */
    uint64_t stop();

    /** Closes the underlying file - called automatically by the destructor */
    void close();

private:
    int _fd;
};
