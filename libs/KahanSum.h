#pragma once

/** Produces a sum with minimal rounding errors.
 * This should be done automatically when summing on loops but it's not done
 * when done in updates like in online statistics.
 */
template <typename T>
class KahanSum {
public:
    KahanSum() {
        clear();
    }

    //! Resets the counter
    inline void clear() {
        _sum = 0;
        _res = 0;
    }

    //! Adds a new value to the sum
    inline void add(double value) {
        T y = value - _res;
        T t = _sum + y;
        _res = (t - _sum) - y;
        _sum = t;
    }

    //! Returns the current sum
    inline T operator()() const {
        return _sum;
    }

    //! Returns the current sum
    inline T sum() const {
        return _sum;
    }

    //! Returns the current residual
    inline T residual() const {
        return _res;
    }

private:
    T _sum;  //! Current sum
    T _res;  //! Current residual
};
