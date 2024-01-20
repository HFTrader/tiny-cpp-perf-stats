#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <vector>

namespace hb {

template <typename Key, typename Value, typename IndexType = std::size_t>
class IndexedMap {
    static constexpr IndexType InvalidIndex = std::numeric_limits<IndexType>::max();
    struct Index {
        IndexType index = InvalidIndex;
    };
    using MapType = std::map<Key, Index>;
    using Vector = std::vector<Value>;
    MapType _map;
    Vector _vec;

public:
    typename Vector::const_iterator begin() const {
        return _vec.begin();
    }
    typename Vector::const_iterator end() const {
        return _vec.end();
    }
    typename Vector::iterator begin() {
        return _vec.begin();
    }
    typename Vector::iterator end() {
        return _vec.end();
    }
    Value& operator[](const Key& key) {
        Index& index(_map[key]);
        if (index.index == InvalidIndex) {
            index.index = _map.size() - 1;
            _vec.resize(_map.size());
        }
        return _vec[index.index];
    };
    Value& operator[](std::size_t index) {
        return _vec[index];
    }
    const Value& operator[](std::size_t index) const {
        return _vec[index];
    }
    IndexType size() const {
        return _vec.size();
    }
};

}  // namespace hb
