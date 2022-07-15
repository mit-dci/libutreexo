#ifndef UTREEXO_TEST_UTILS_H
#define UTREEXO_TEST_UTILS_H

#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <random>
#include <vector>

using Hash = std::array<uint8_t, 32>;
using Leaf = std::pair<Hash, bool>;

void SetHash(Hash& hash, int num);
void CreateTestLeaves(std::vector<Leaf>& leaves, int count, int offset);
void CreateTestLeaves(std::vector<Leaf>& leaves, int count);
Hash HashFromStr(const std::string& hex);

// https://github.com/bitcoin/bitcoin/blob/7f653c3b22f0a5267822eec017aea6a16752c597/src/util/strencodings.cpp#L580
template <class T>
static std::string HexStr(const T s)
{
    std::string rv;
    static constexpr char hexmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    rv.reserve(s.size() * 2);
    for (uint8_t v : s) {
        rv.push_back(hexmap[v >> 4]);
        rv.push_back(hexmap[v & 15]);
    }
    return rv;
}

static void MarkLeavesAsMemorable(std::vector<Leaf>& leaves)
{
    for (Leaf& leaf : leaves) {
        leaf.second = true;
    }
}

static std::vector<Leaf> CopyLeavesAsMemorable(std::vector<Leaf>& leaves)
{
    std::vector<Leaf> copy{leaves.begin(), leaves.end()};
    MarkLeavesAsMemorable(copy);
    return copy;
}
#endif
