#pragma once

#include <cstdint>
#include <array>
#include <iostream>

#if defined(__GNUC__) && defined(__x86_64__)
//! Returns the most significant bit
inline uint32_t msb(uint64_t value) {
    if (value == 0) return 0;
    return 64 - __builtin_clzll(value);
}
#else
//! Slower method
inline uint32_t msb(uint64_t value) {
    uint32_t count = 0;
    while (value > 0) {
        count++;
        value >>= 1;
    }
    return count;
}
#endif

/**
 * Constructs a power-of-two histogram with NDB intermediate divisions
 * for added precision. Each added division doubles the size of the histogram.
 * The initial size is 64 (for NDB=0).
 * Uses the theory developed at "Generating Semilog Series Fast" on Substack
 * https://lucisqr.substack.com/p/generating-semilog-series-fast
 */
template <uint32_t NDB>
class MicroStats {
public:
    //! The number of available bins possible in a 64-bit range
    static constexpr uint32_t NUMBINS = (64 - NDB) << NDB;

    //! Mask used extensively in the calculations
    static constexpr uint64_t MASK = (1ULL << NDB) - 1;

    //! Default constructor initializes the structures with proper ranges
    MicroStats() {
        init();
    }

    //! Default destructor, nothing to do
    ~MicroStats() = default;

    //! Adds a value to the histogram
    void add(uint64_t value) {
        uint32_t binnum = calcbin(value);
        Bin& bin(bins[binnum]);
        bin.add(value);
        totalcount += 1;
        totalsum += value;
    }

    //! Clears the histogram
    void clear() {
        for (uint32_t j = 0; j < bins.size(); ++j) {
            Bin& bin(bins[j]);
            bin.clear();
        }
        totalcount = 0;
        totalsum = 0;
    }

    //! Returns the respective percentile value
    double percentile(double pct) const {
        double goal = pct * totalcount / 100.0;
        uint64_t count = 0;
        uint32_t index = 0;
        for (const Bin& bin : bins) {
            if (count + bin.count >= goal) {
                if (bin.count == 0) {
                    // just send the middle
                    return (bin.range.from + bin.range.to) / 2;
                }
                // This assumes a uniform distribution
                double excess = goal - count;
                double ratio = excess / bin.count;
                double res = bin.range.to * ratio + bin.range.from * (1.0 - ratio);
                return res;
            }
            count += bin.count;
        }
        // Bad luck
        return -1;
    }

    //! Prints the histogram's values as percentiles
    void print(std::ostream& oss) const {
        for (double v : {1, 10, 25, 50, 75, 90, 99}) {
            oss << v << "%," << percentile(v) << ",";
        }
    }

    //! Convenience ostream operator
    friend inline std::ostream& operator<<(std::ostream& out, const MicroStats& ms) {
        ms.print(out);
        return out;
    }

    //! Computes the respective bin given a value
    static uint32_t calcbin(uint64_t value) {
        if (value < (1 << NDB)) return value;
        uint32_t numbits = msb(value);
        return ((numbits - NDB) << NDB) + ((value >> (numbits - (NDB + 1))) & MASK);
    }

    //! The (inclusive) integer range of a bin
    struct Range {
        uint64_t from;
        uint64_t to;
    };

    //! Computes the range of a bin given its ID
    static Range calcrange(uint32_t bin) {
        if (bin < (1 << NDB)) return Range{bin, bin};
        uint32_t partition = bin & MASK;
        uint32_t numbits = (bin >> NDB) + NDB;
        uint64_t base = 1ULL << (numbits - 1);
        uint64_t offset = partition << (numbits - (NDB + 1));
        return Range{base + offset, base + (base >> NDB) + offset - 1};
    }

private:
    //! Initializes the histogram with the calculated ranges
    void init() {
        for (uint32_t j = 0; j < bins.size(); ++j) {
            Bin& bin(bins[j]);
            bin.clear();
            bin.range = calcrange(j);
        }
        totalcount = 0;
        totalsum = 0;
    }

    //! Store the bin's statistics
    struct Bin {
        Range range;
        uint32_t count = 0;
        double sum = 0;
        double sum2 = 0;
        void clear() {
            count = 0;
            sum = 0;
            sum2 = 0;
        }
        void add(uint64_t value) {
            count++;
            sum += value;
            sum2 += double(value) * value;
        }
    };

    std::array<Bin, NUMBINS> bins;  //! Collection of bins
    double totalsum = 0;            //! Total sum of all values inserted
    uint64_t totalcount = 0;        //! Total count of all values inserted
};
