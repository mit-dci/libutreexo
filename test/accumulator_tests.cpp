#include "util/strencodings.h"
#include <accumulator.h>
#include <boost/test/unit_test.hpp>
#include <pollard.h>
#include <ram_forest.h>
#include <state.h>
#include <uint256.h>
BOOST_AUTO_TEST_SUITE(AccumulatorTests)

class TestLeaf : public Accumulator::Leaf
{
private:
    uint256 h;

public:
    TestLeaf(uint8_t num)
    {
        h.begin()[0] = num;
    }

    uint256 hash() const
    {
        return h;
    }

    bool remember() const
    {
        return false;
    }
};

BOOST_AUTO_TEST_CASE(simple_full)
{
    ForestState state(0);
    Accumulator* full = (Accumulator*)new RamForest(state);

    auto leaves = std::vector<std::shared_ptr<Accumulator::Leaf>>();
    for (int i = 0; i < 15; i++) {
        leaves.push_back(std::shared_ptr<Accumulator::Leaf>((Accumulator::Leaf*)new TestLeaf(i + 1)));
    }
    full->modify(leaves, std::vector<uint64_t>());
    leaves.clear();
    full->modify(leaves, {0, 2, 3, 9});
    delete full;
}

BOOST_AUTO_TEST_CASE(simple_pruned)
{
    ForestState state(0);
    Accumulator* pruned = (Accumulator*)new Pollard(state);

    auto leaves = std::vector<std::shared_ptr<Accumulator::Leaf>>();
    for (int i = 0; i < 15; i++) {
        leaves.push_back(std::shared_ptr<Accumulator::Leaf>((Accumulator::Leaf*)new TestLeaf(i + 1)));
    }
    pruned->modify(leaves, std::vector<uint64_t>());
    leaves.clear();
    pruned->modify(leaves, {0, 2, 3, 9});

    delete pruned;
}

BOOST_AUTO_TEST_SUITE_END()
