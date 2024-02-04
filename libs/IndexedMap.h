#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <vector>

/** An array that keeps a keymap per entry for log(N) or O(1) lookups
 * The Map has to conform to the signature <Key,Value,...>
 */
template <typename Key, typename Value, typename IndexType = std::size_t,
          template <typename, typename, typename...> class MapContainer = std::map>
class IndexedMap {
    //! Invalid is the maximum value of the given type
    static constexpr IndexType InvalidIndex = std::numeric_limits<IndexType>::max();
    //! An integral type that initializes to an invalid value
    struct Index {
        IndexType index = InvalidIndex;
    };
    //!
    using MapType = MapContainer<Key, Index>;
    using Vector = std::vector<Value>;
    MapType _map;
    Vector _vec;

public:
    //! const iterator - iterates on the vector
    typename Vector::const_iterator begin() const {
        return _vec.begin();
    }
    typename Vector::const_iterator end() const {
        return _vec.end();
    }

    //! iterator
    typename Vector::iterator begin() {
        return _vec.begin();
    }
    typename Vector::iterator end() {
        return _vec.end();
    }

    //! Returns or creates a value given a key
    Value& operator[](const Key& key) {
        Index& index(_map[key]);
        if (index.index == InvalidIndex) {
            index.index = _map.size() - 1;
            _vec.resize(_map.size());
        }
        return _vec[index.index];
    };

    //! Returns a value given an index
    Value& operator[](std::size_t index) {
        return _vec[index];
    }

    //! Returns a value given an index - const version
    const Value& operator[](std::size_t index) const {
        return _vec[index];
    }

    //! Returns the size of the container
    IndexType size() const {
        return _vec.size();
    }
};
