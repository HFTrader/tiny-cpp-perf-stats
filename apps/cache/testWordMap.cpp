#include <string>
#include <string_view>
#include <map>
#include <set>
#include <algorithm>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/container/flat_map.hpp>

#include "Datasets.h"
#include "StringUtils.h"
#include "Snapshot.h"
#include "DateUtils.h"
#include "Allocators.h"

struct Counter {
    uint64_t value = 0;
    operator uint64_t() const {
        return value;
    }
    uint64_t operator++() {
        value++;
        return value;
    }
    uint64_t operator++(int) {
        uint64_t tmp = value;
        value++;
        return tmp;
    }
};

// The actual test run, templated by map type
template <template <typename Key, typename Value, typename AllocType> class MapType,
          template <typename ValueType> class AllocatorType>
void testme(const std::string& testname, Snapshot& snap,
            const std::vector<std::string_view>& allwords, uint32_t numwords,
            double runsecs) {
    using KeyValuePair = std::pair<std::string_view, Counter>;
    MapType<std::string_view, Counter, AllocatorType<KeyValuePair>> words;

    double start = now();
    snap.start();
    uint64_t counter = 0;
    do {
        for (std::string_view w : allwords) words[w]++;
        counter++;
    } while (now() < start + runsecs);
    uint64_t total = 0;
    for (auto iw : words) {
        total += iw.second;
    }
    snap.stop(testname, numwords, counter * allwords.size());

#ifdef DEBUG
#endif
}

// All our containers, templated by allocator type
template <class Key, class Value, class Allocator>
using BoostFlatMapType =
    boost::container::flat_map<Key, Value, std::less<Key>, Allocator>;
template <class Key, class Value, class Allocator>
using StdMapType = std::map<Key, Value, std::less<Key>, Allocator>;
template <class Key, class Value, class Allocator>
using StdHashMapType =
    std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>;

// All ou allocators
template <typename ValueType>
using stdalloc = std::allocator<ValueType>;
template <typename ValueType>
using wrapped = retail_allocator<ValueType, standard_allocator>;
template <typename ValueType>
using mmaped = retail_allocator<ValueType, mmap_allocator>;
template <typename ValueType>
using transp = retail_allocator<ValueType, transparent_allocator>;
template <typename ValueType>
using hugepage = retail_allocator<ValueType, hugepage_allocator>;
template <typename ValueType>
using boostpmr = BoostAllocator<ValueType>;

int main() {
    // Read the text and make all uppercase
    std::string text = toupper(datasets::bible);

    // Count unique words in the text and store them in a vector
    std::map<std::string_view, Counter> wordcount;
    std::vector<std::string_view> allwords;
    split(text, '\n', [&allwords, &wordcount](std::string_view line) -> bool {
        // The format is like
        // GEN 1:1 In the beginning God created the heaven and the earth.
        auto [book, loc, verse] = split<3>(line, ' ');

        // Split the verse where the separator is anything not an alphanumeric digit
        split(verse, isalphanum<false>(), [&allwords, &wordcount](std::string_view w) {
            // Increment the counter
            allwords.push_back(w);
            wordcount[w]++;
            // Iterate forever
            return true;
        });
        // Iterate forever
        return true;
    });

    // Map in reverse by words with the same count
    uint64_t total = 0;
    std::map<uint64_t, std::set<std::string_view>, std::greater<uint64_t>> countmap;
    for (auto iw : wordcount) {
        countmap[iw.second].insert(iw.first);
        total += iw.second;
    }

    // Print stats
    printf("\nRead %ld words, %ld unique\n", total, wordcount.size());

    // List all words that make up 50% of the text
    uint64_t counter = 0;
    for (auto [count, wordlist] : countmap) {
        // update count
        counter += count * wordlist.size();

        // compute cumulative stats
        double pct = double(count * 100) / total;
        double cumpct = double(counter * 100) / total;

        // print stats
        printf("%5.1f%% %5.1f%%   %5ld:", cumpct, pct, count);

        // List words
        char ch = ' ';
        for (std::string_view word : wordlist) {
            printf("%c%.*s", ch, (int)word.size(), word.data());
            ch = ',';
        }
        printf("\n");

        // Break if limit reached
        if (cumpct > 50) break;
    }

    double runsecs = 0.5;

    // Start snapshotting
    Snapshot snap(1);
    for (unsigned numwords = 500; numwords < allwords.size(); numwords += numwords / 4) {
        // Create a word filter with the N most popular words
        std::set<std::string_view> wordfilter;
        uint64_t counter = 0;
        for (auto [count, wordlist] : countmap) {
            for (std::string_view w : wordlist) {
                wordfilter.insert(w);
                counter++;
                if (counter >= numwords) break;
            }
            if (counter >= numwords) break;
        }

        // Filter the original set with only the N most popular
        std::vector<std::string_view> words;
        for (std::string_view w : allwords) {
            if (wordfilter.find(w) != wordfilter.end()) {
                words.push_back(w);
            }
        }

        testme<StdMapType, stdalloc>("std::map<stdalloc>", snap, words, numwords,
                                     runsecs);
        testme<StdMapType, wrapped>("std::map<wrapped>", snap, words, numwords, runsecs);
        testme<StdMapType, mmaped>("std::map<mmaped>", snap, words, numwords, runsecs);
        testme<StdMapType, transp>("std::map<transp>", snap, words, numwords, runsecs);
        testme<StdMapType, hugepage>("std::map<hugepage>", snap, words, numwords,
                                     runsecs);
        testme<StdMapType, boostpmr>("std::map<boostpmr>", snap, words, numwords,
                                     runsecs);

        testme<BoostFlatMapType, stdalloc>("boost::flat_map<stdalloc>", snap, words,
                                           numwords, runsecs);
        testme<BoostFlatMapType, wrapped>("boost::flat_map<wrapped>", snap, words,
                                          numwords, runsecs);
        testme<BoostFlatMapType, mmaped>("boost::flat_map<mmaped>", snap, words, numwords,
                                         runsecs);
        testme<BoostFlatMapType, transp>("boost::flat_map<transp>", snap, words, numwords,
                                         runsecs);
        testme<BoostFlatMapType, hugepage>("boost::flat_map<hugepage>", snap, words,
                                           numwords, runsecs);
        testme<BoostFlatMapType, boostpmr>("boost::flat_map<boostpmr>", snap, words,
                                           numwords, runsecs);

        testme<StdHashMapType, stdalloc>("std::unordered_map<stdalloc>", snap, words,
                                         numwords, runsecs);
        testme<StdHashMapType, wrapped>("std::unordered_map<wrapped>", snap, words,
                                        numwords, runsecs);
        testme<StdHashMapType, mmaped>("std::unordered_map<mmaped>", snap, words,
                                       numwords, runsecs);
        testme<StdHashMapType, transp>("std::unordered_map<transp>", snap, words,
                                       numwords, runsecs);
        testme<StdHashMapType, hugepage>("std::unordered_map<hugepage>", snap, words,
                                         numwords, runsecs);
        testme<StdHashMapType, boostpmr>("std::unordered_map<boostpmr>", snap, words,
                                         numwords, runsecs);
    }

    snap.summary("WordMap");
}
