#include "util/strencodings.h"
#include <accumulator.h>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <nodepool.h>
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

BOOST_AUTO_TEST_CASE(simple_add)
{
    ForestState state(0);
    Accumulator* full = (Accumulator*)new Pollard(state, 160);
    auto start = std::chrono::high_resolution_clock::now();
    auto leaves = std::vector<std::shared_ptr<Accumulator::Leaf>>();
    // NodePtr<Accumulator::Node> new_root;
    for (int i = 0; i < 64; i++) {
        leaves.push_back(std::shared_ptr<Accumulator::Leaf>((Accumulator::Leaf*)new TestLeaf(i + 1)));
        //full->NewLeaf(uint256S(""));
        //    new_root = full->NewLeaf(leaves.back()->Hash());
    }

    full->Modify(leaves, std::vector<uint64_t>());
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    //std::cout << "Elapsed time: " << elapsed.count() << " s\n";
    delete full;
}

BOOST_AUTO_TEST_CASE(simple_full)
{
    ForestState state(0);
    Accumulator* full = (Accumulator*)new RamForest(state, 32);

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
    Accumulator* pruned = (Accumulator*)new Pollard(state, 32);

    auto leaves = std::vector<std::shared_ptr<Accumulator::Leaf>>();
    for (int i = 0; i < 15; i++) {
        leaves.push_back(std::shared_ptr<Accumulator::Leaf>((Accumulator::Leaf*)new TestLeaf(i + 1)));
    }
    pruned->Modify(leaves, std::vector<uint64_t>());
    leaves.clear();
    pruned->Modify(leaves, {0, 2, 3, 9});

    delete pruned;
}

BOOST_AUTO_TEST_CASE(batchproof_serialization)
{
    std::vector<uint8_t> proof_bytes;
    Accumulator::BatchProof proof1{{0, 1, 3}, {uint256S("0x1"), uint256S("0x2")}};
    proof1.Serialize(proof_bytes);

    Accumulator::BatchProof proof2;
    BOOST_CHECK(proof2.Unserialize(proof_bytes));

    BOOST_CHECK(proof1 == proof2);
}

BOOST_AUTO_TEST_CASE(simple_verify)
{
    ForestState state(0);
    Accumulator* full = (Accumulator*)new RamForest(state, 32);

    auto leaves = std::vector<std::shared_ptr<Accumulator::Leaf>>();
    for (int i = 0; i < 15; i++) {
        leaves.push_back(std::shared_ptr<Accumulator::Leaf>((Accumulator::Leaf*)new TestLeaf(i + 1)));
    }
    full->Modify(leaves, std::vector<uint64_t>());

    std::vector<uint256> target_hashes;
    target_hashes.push_back(leaves.at(0)->Hash());
    target_hashes.push_back(leaves.at(2)->Hash());
    target_hashes.push_back(leaves.at(3)->Hash());
    target_hashes.push_back(leaves.at(9)->Hash());

    Accumulator::BatchProof proof = full->Prove(target_hashes);
    //proof.Print();

    auto roots = full->Roots();
    std::reverse(roots.begin(), roots.end());

    BOOST_CHECK(proof.Verify(state, roots, target_hashes));

    delete full;
}

BOOST_AUTO_TEST_SUITE_END()
