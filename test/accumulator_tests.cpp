#include "util/strencodings.h"
#include <accumulator.h>
#include <boost/test/unit_test.hpp>
#include <pollard.h>
#include <ram_forest.h>
#include <state.h>
#include <uint256.h>
BOOST_AUTO_TEST_SUITE(accumulator_tests)

class TestLeaf : public Accumulator::Leaf
{
private:
    uint256 h;

public:
    TestLeaf(uint8_t num)
    {
        h.begin()[0] = num;
    }

    uint256 Hash() const
    {
        return h;
    }

    bool Remember() const
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
    full->Modify(leaves, std::vector<uint64_t>());
    leaves.clear();
    full->Modify(leaves, {0, 2, 3, 9});
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
    pruned->Modify(leaves, std::vector<uint64_t>());
    leaves.clear();
    pruned->Modify(leaves, {0, 2, 3, 9});

    delete pruned;
}

BOOST_AUTO_TEST_CASE(simple_verify)
{
    ForestState state(0);
    Accumulator* full = (Accumulator*)new RamForest(state);

    auto leaves = std::vector<std::shared_ptr<Accumulator::Leaf>>();
    for (int i = 0; i < 15; i++) {
        leaves.push_back(std::shared_ptr<Accumulator::Leaf>((Accumulator::Leaf*)new TestLeaf(i + 1)));
    }
    full->Modify(leaves, std::vector<uint64_t>());

    Accumulator::BatchProof proof = full->Prove({0, 2, 3, 9});
    proof.Print();

    auto roots = full->Roots();
    std::reverse(roots.begin(), roots.end());

    std::vector<uint256> target_hashes;
    target_hashes.push_back(leaves.at(0)->Hash());
    target_hashes.push_back(leaves.at(2)->Hash());
    target_hashes.push_back(leaves.at(3)->Hash());
    target_hashes.push_back(leaves.at(9)->Hash());

    BOOST_CHECK(proof.Verify(state, roots, target_hashes));

    delete full;
}

BOOST_AUTO_TEST_SUITE_END()
