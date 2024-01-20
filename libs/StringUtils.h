#pragma once

#include <string_view>
#include <array>
#include <algorithm>
#include <cstring>

template <size_t N>
struct TinyString {
    char buffer[N];
    friend std::ostream& operator<<(std::ostream& out, const TinyString<N>& res) {
        out << res.buffer;
        return out;
    }
};

// Dearly missing split function from the STL
// I should have it as string_view instead but it's not ubiquitous yet
template <typename SepFn, typename CallFn>
static inline void split(std::string_view str, SepFn&& issep, CallFn&& callback) {
    auto skipuntil = [&str](std::string_view::iterator it,
                            auto fn) -> std::string_view::iterator {
        for (; it != str.end(); ++it) {
            if (fn(it)) return it;
        }
        return it;
    };
    auto oneof = [&issep](std::string_view::iterator it) { return issep(it); };
    auto notof = [&issep](std::string_view::iterator it) { return !issep(it); };
    auto start = str.begin();
    while (start != str.end()) {
        start = skipuntil(start, notof);
        if (start == str.end()) break;
        auto finish = skipuntil(start, oneof);
        if (start != finish) {
            if (!callback(std::string_view(start, finish - start))) break;
        }
        start = finish;
    }
}

struct ischar {
    char ch;
    ischar(char c) : ch(c) {
    }
    bool operator()(std::string_view::iterator ic) const {
        return *ic == ch;
    }
};

template <bool RESULT>
struct isalphanum {
    isalphanum() {
    }
    bool operator()(std::string_view::iterator ic) const {
        return (std::isalnum((int)*ic) != 0) == RESULT;
    }
};

struct anyof {
    std::string_view sep;
    anyof(std::string_view sv) : sep(sv) {
    }
    bool operator()(std::string_view::iterator ic) const {
        for (char ch : sep) {
            if (ch == *ic) return true;
        }
        return false;
    }
};

template <typename CallFn>
static inline void split(std::string_view str, char ch, CallFn&& callback) {
    split(str, ischar(ch), callback);
}

template <int N, typename Fn>
static inline std::array<std::string_view, N> split(std::string_view str, Fn&& sepfn) {
    std::array<std::string_view, N> result;
    size_t counter = 0;
    split(str, sepfn, [&result, &counter, str](std::string_view s) -> bool {
        if (counter + 1 < N) {
            result[counter] = s;
            counter++;
            return true;
        }
        std::size_t length = str.end() - s.begin();
        result[counter] = std::string_view(s.begin(), length);
        counter++;
        return false;
    });
    for (; counter < N; ++counter) {
        result[counter] = std::string_view();
    }
    return result;
}

std::string toupper(std::string_view str) {
    std::string text(str);
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return text;
}