#pragma once

template< typename T >
class KahanSum
{
public:
	KahanSum() { clear(); }

	inline void clear() {
		_sum = 0;
		_res = 0;
	}

	inline void add(double value) {
		T y = value - _res;
		T t = _sum + y;
		_res = (t - _sum) - y;
		_sum = t;
	}

	inline T operator()() const {
		return _sum;
	}

	inline T sum() const {
		return _sum;
	}

	inline T residual() const {
		return _res;
	}

private:
	T _sum;
	T _res;
};
