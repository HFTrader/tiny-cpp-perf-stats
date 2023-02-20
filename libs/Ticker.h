#pragma once
#include <cstring>
#include <string>
#include <cstdint>
#include <string_view>

// Represents a ticker
// It has fast comparison operators using the fact that
// the ticker value has 8 bytes
struct Ticker {
    union {
        uint64_t uval;
        char name[8];
    };
    bool operator<(const Ticker& rhs) const {
        return uval < rhs.uval;
    }
    bool operator==(const Ticker& rhs) const {
        return uval == rhs.uval;
    }
    Ticker& operator=(const std::string& rhs) {
        memset(name, 0, sizeof(name));
        strncpy(name, rhs.c_str(), sizeof(name));
        return *this;
    }
    Ticker& operator=(const std::string_view rhs) {
        memset(name, 0, sizeof(name));
        memcpy(name, rhs.data(), std::min(rhs.size(), sizeof(name)));
        return *this;
    }
    Ticker& operator=(const Ticker& rhs) {
        if (&rhs != this) {
            uval = rhs.uval;
        }
        return *this;
    }
    friend std::ostream& operator<<(std::ostream& out, const Ticker& ticker) {
        return out << std::string(ticker.name);
    }
};

// Implements std::hash for the Ticker object so it can be used
// as key in STL containers
template <>
struct std::hash<Ticker> {
    std::size_t operator()(Ticker const& s) const noexcept {
        return h(s.uval);
    }
    std::hash<uint64_t> h;
};
