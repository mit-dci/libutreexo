#include <accumulator.h>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <cstring>
#include <nodepool.h>
#include <pollard.h>
#include <ram_forest.h>
#include <state.h>
BOOST_AUTO_TEST_SUITE(accumulator_tests)

using namespace utreexo;

void SetHash(Hash& hash, int num)
{
    for (uint8_t& byte : hash) {
        byte = 0;
    }

    hash[0] = num;
    hash[1] = num >> 8;
    hash[2] = num >> 16;
    hash[3] = num >> 24;
    hash[4] = 0xFF;
}

void CreateTestLeaves(std::vector<Leaf>& leaves, int count)
{
    for (int i = 0; i < count; i++) {
        Hash hash;
        SetHash(hash, i);
        leaves.emplace_back(std::move(hash), false);
    }
}

BOOST_AUTO_TEST_CASE(simple_add)
{
    ForestState state(0);
    Accumulator* full = (Accumulator*)new Pollard(state, 160);
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 64);

    full->Modify(leaves, std::vector<uint64_t>());
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;

    delete full;
}

BOOST_AUTO_TEST_CASE(simple_full)
{
    ForestState state(0);
    Accumulator* full = (Accumulator*)new RamForest(state, 32);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 16);
    // Add test leaves, dont delete any.
    full->Modify(leaves, std::vector<uint64_t>());
    // Delete some leaves, dont add any new ones.
    leaves.clear();
    full->Modify(leaves, {0, 2, 3, 9});

    delete full;
}

BOOST_AUTO_TEST_CASE(simple_pruned)
{
    ForestState state(0);
    Accumulator* full = (Accumulator*)new Pollard(state, 32);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 16);

    // Add test leaves, dont delete any.
    full->Modify(leaves, std::vector<uint64_t>());
    // Delete some leaves, dont add any new ones.
    leaves.clear();
    full->Modify(leaves, {0, 2, 3, 9});

    delete full;
}

BOOST_AUTO_TEST_CASE(batchproof_serialization)
{
    ForestState state(0);
    Accumulator* full = (Accumulator*)new RamForest(state, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 32);

    full->Modify(leaves, std::vector<uint64_t>());

    std::vector<uint8_t> proof_bytes;
    Accumulator::BatchProof proof1;
    full->Prove(proof1, {leaves[0].first, leaves[1].first});
    proof1.Serialize(proof_bytes);

    Accumulator::BatchProof proof2;
    BOOST_CHECK(proof2.Unserialize(proof_bytes));
    BOOST_CHECK(proof1 == proof2);
}

BOOST_AUTO_TEST_SUITE_END()
