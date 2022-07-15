#include "utreexo.h"
#include "utils.h"
#include "crypto/sha512.h"

#include <boost/test/unit_test.hpp>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

BOOST_AUTO_TEST_SUITE(swapless_tests)

using namespace utreexo;
using namespace utreexo::detail;

using BatchProof = BatchProof<Hash>;
using Hash = std::array<uint8_t, 32>;

void PrintLeaves(Accumulator<Hash>& acc)
{
    std::vector<Hash> leaves{acc.GetCachedLeaves()};
    std::cout << "cached leaves: " << std::endl;
    for (const Hash& hash : leaves) {
        std::cout << HexStr(hash) << std::endl;
    }
}

/*void TestUndo(Accumulator<Hash>& pruned, Accumulator<Hash>& full, const std::vector<Leaf>& leaves, const std::vector<Hash>& deletions)
{
    BatchProof proof;
    if (!deletions.empty()) {
        BOOST_CHECK(full.Prove(proof, deletions));
        BOOST_CHECK(pruned.Verify(proof, deletions));
    }

    auto state_before = pruned.GetState();
    BOOST_CHECK(state_before == full.GetState());

    BOOST_CHECK(full.Modify(leaves, proof.GetSortedTargets()));
    BOOST_CHECK(pruned.Modify(leaves, proof.GetSortedTargets()));
    BOOST_CHECK(pruned.GetState() == full.GetState());
    if (!leaves.empty() && !deletions.empty()) {
        BOOST_CHECK(state_before != full.GetState());
    }

    BOOST_CHECK(full.Undo(std::get<0>(state_before), std::get<1>(state_before), proof, deletions));
    BOOST_CHECK(pruned.Undo(std::get<0>(state_before), std::get<1>(state_before), proof, deletions));

    BOOST_CHECK(pruned.GetState() == full.GetState());
    BOOST_CHECK(state_before == full.GetState());
}

BOOST_AUTO_TEST_CASE(undo)
{
    std::unique_ptr<Accumulator<Hash>> full = MakeEmpty<Hash>();
    std::unique_ptr<Accumulator<Hash>> pruned = MakeEmpty<Hash>();

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);

    BOOST_CHECK(pruned->Modify(leaves, {}));
    BOOST_CHECK(pruned->GetCachedLeaves().size() == 0);
    for (Leaf& leaf : leaves) {
        leaf.second = true;
    }
    BOOST_CHECK(full->Modify(leaves, {}));
    BOOST_CHECK(full->GetCachedLeaves().size() == 15);

    std::vector<Hash> deletions{
        leaves[0].first,
        leaves[1].first,
        leaves[4].first,
        leaves[13].first,
        leaves[14].first,
    };

    // Test undoing nothing.
    TestUndo(*pruned, *full, {}, {});
    // Test undoing removal.
    TestUndo(*pruned, *full, {}, deletions);
    // Test undoing additions.
    std::vector<Leaf> more_leaves;
    CreateTestLeaves(more_leaves, 1, 15);
    more_leaves.back().second = true;
    TestUndo(*pruned, *full, more_leaves, {});
    // Test both undoing and additions.
    TestUndo(*pruned, *full, more_leaves, deletions);
}*/

/*BOOST_AUTO_TEST_CASE(simple)
{
    for (int num_leaves = 1; num_leaves < 256; ++num_leaves) {
        std::unique_ptr<Accumulator<Hash>> full = MakeEmpty<Hash>();
        std::unique_ptr<Accumulator<Hash>> pruned = MakeEmpty<Hash>();

        std::vector<Leaf> leaves;
        CreateTestLeaves(leaves, num_leaves);

        BOOST_CHECK(pruned->Modify(leaves, {}));
        BOOST_CHECK(pruned->GetCachedLeaves().size() == 0);
        for (Leaf& leaf : leaves) {
            leaf.second = true;
        }
        BOOST_CHECK(full->Modify(leaves, {}));
        BOOST_CHECK(full->GetCachedLeaves().size() == num_leaves);

        for (Leaf& leaf : leaves) {
            BatchProof proof;
            BOOST_CHECK(!pruned->Prove(proof, {leaf.first}));
            BOOST_CHECK(full->Prove(proof, {leaf.first}));
            BOOST_CHECK(pruned->Verify(proof, {leaf.first}));
            BOOST_CHECK(pruned->GetCachedLeaves().size() == 1);
            BOOST_CHECK(pruned->Prove(proof, {leaf.first}));
            pruned->Uncache(leaf.first);
            BOOST_CHECK(!pruned->Prove(proof, {leaf.first}));
        }

        std::vector<int> leaf_positions;
        for (int i = 0; i < 16; ++i) {
            leaf_positions.push_back(std::rand() % leaves.size());
        }
        std::sort(leaf_positions.begin(), leaf_positions.end());
        auto last = std::unique(leaf_positions.begin(), leaf_positions.end());
        leaf_positions.erase(last, leaf_positions.end());

        std::vector<Hash> target_hashes;
        for (int leaf_pos : leaf_positions) {
            target_hashes.push_back(leaves[leaf_pos].first);
        }

        BatchProof proof;
        BOOST_CHECK(!pruned->Prove(proof, target_hashes));
        BOOST_CHECK(full->Prove(proof, target_hashes));

        if (proof.GetHashes().size() > 0) {
            std::vector<Hash> modified_hashes = proof.GetHashes();
            // Fill the last hash with zeros.
            // This should cause verification to fail.
            modified_hashes.front().fill(0);
            BatchProof invalid(proof.GetSortedTargets(), modified_hashes);
            BOOST_CHECK(!pruned->Verify(invalid, target_hashes));
        }

        BOOST_CHECK(pruned->Verify(proof, target_hashes));
        BOOST_CHECK(pruned->GetCachedLeaves().size() == target_hashes.size());
        BOOST_CHECK(pruned->Prove(proof, target_hashes));
        for (const Hash& leaf_hash : target_hashes) {
            BOOST_CHECK(pruned->Prove(proof, {leaf_hash}));
            pruned->Uncache(leaf_hash);
            BOOST_CHECK(!pruned->Prove(proof, {leaf_hash}));
        }
        BOOST_CHECK(!pruned->Prove(proof, target_hashes));

        BOOST_CHECK(full->Prove(proof, target_hashes));
        BOOST_CHECK(pruned->Verify(proof, target_hashes));

        auto state = pruned->GetState();
        std::vector<Leaf> additions;
        CreateTestLeaves(additions, std::rand() % leaves.size(), leaves.size());
        BOOST_CHECK(pruned->Modify({}, proof.GetSortedTargets()));
        BOOST_CHECK(state != pruned->GetState());
        BOOST_CHECK(pruned->Undo(std::get<0>(state), std::get<1>(state), proof, target_hashes));
        BOOST_CHECK(state == pruned->GetState());
    }
}*/

BOOST_AUTO_TEST_SUITE_END()
