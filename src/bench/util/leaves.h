#ifndef UTREEXO_BENCH_UTIL_LEAVES_H
#define UTREEXO_BENCH_UTIL_LEAVES_H

#include "include/accumulator.h"

#include <vector>

using namespace utreexo;

// Copied from src/test/accumulator_tests.cpp
static void SetHash(Hash& hash, int num)
{
    hash[0] = num;
    hash[1] = num >> 8;
    hash[2] = num >> 16;
    hash[3] = num >> 24;
    hash[4] = 0xFF;
}
// Copied from src/test/accumulator_tests.cpp
static void CreateTestLeaves(std::vector<Leaf>& leaves, int count, int offset)
{
    for (int i = 0; i < count; i++) {
        Hash hash = {}; // initialize all elements to 0
        SetHash(hash, offset + i);
        leaves.emplace_back(std::move(hash), false);
    }
}
// Copied from src/test/accumulator_tests.cpp
static void CreateTestLeaves(std::vector<Leaf>& leaves, int count)
{
    CreateTestLeaves(leaves, count, 0);
}

// Fisher-Yates shuffle, https://stackoverflow.com/a/9345144/5800072
template <class bidiiter>
bidiiter random_unique(bidiiter begin, bidiiter end, size_t num_random)
{
    size_t left = std::distance(begin, end);
    while (num_random--) {
        bidiiter r = begin;
        std::advance(r, rand() % left);
        std::swap(*begin, *r);
        ++begin;
        --left;
    }
    return begin;
}

#endif // UTREEXO_BENCH_UTIL_LEAVES_H
