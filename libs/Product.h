#pragma once
#include <vector>

template <typename T>
struct Product {
    typedef std::vector<T> Result;

    // true while there are more solutions
    bool first;
    bool completed;

    // count how many generated
    T count;

    // The total number of elements
    std::vector<T> N;

    // Places to arrange
    T R;

    // The current combination
    Result current;

    // initialize status
    template <typename Container>
    Product(const Container& Nt) {
        init(Nt);
    }

    T operator[](size_t index) {
        return current[index];
    }

    template <typename Container>
    void init(const Container& Nt) {
        R = Nt.size();
        completed = false;
        first = true;
        count = 0;
        current.resize(R);
        N.resize(R);
        for (T index = 0; index < R; ++index) {
            current[index] = 0;
            N[index] = Nt[index];
        }
    }

    // get current and compute next combination
    bool next() {
        if (completed) return false;

        // find what to increment
        if (!first) {
            T i = R;
            while (i > 0) {
                i -= 1;
                // can increment the current one?
                if (++current[i] < N[i]) {
                    // yes, go incrementing forward
                    while (++i < R) current[i] = 0;
                    ++count;
                    return true;
                }
                // if it's here it's because it could not increment
                // so we go back to the previous
            }
            completed = true;
            return false;
        }
        count = 1;
        first = false;
        return true;
    }
};
