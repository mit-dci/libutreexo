#include "accumulator.h"
#include "batchproof.h"
#include "state.h"
#include "utils.h"

#include <boost/test/unit_test.hpp>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

BOOST_AUTO_TEST_SUITE(swapless_tests)


using namespace utreexo;

using BatchProof = BatchProof<Hash>;

#define CHECK_VECTORS_EQUAL(c1, c2)                                                                    \
    {                                                                                                  \
        auto c1_copy{c1};                                                                              \
        auto c2_copy{c2};                                                                              \
        BOOST_CHECK_EQUAL_COLLECTIONS(c1_copy.begin(), c1_copy.end(), c2_copy.begin(), c2_copy.end()); \
    }

BOOST_AUTO_TEST_CASE(proof_positions)
{
    ForestState state(15);
    /*
     * xx
     * |-------------------------------\
     * 28                              xx
     * |---------------\               |---------------\
     * 24              25              26              xx
     * |-------\       |-------\       |-------\       |-------\
     * 16      17      18      19      20      21      22      xx
     * |---\   |---\   |---\   |---\   |---\   |---\   |---\   |---\
     * 00  01  02  03  04  05  06  07  08  09  10  11  12  13  14  xx
     */

    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({}), std::vector<uint64_t>{});
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({0}), std::vector<uint64_t>({1, 17, 25}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({14, 22, 26, 28}), std::vector<uint64_t>({}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}), std::vector<uint64_t>({}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({0, 2, 4, 6, 8, 10, 12, 14}), std::vector<uint64_t>({1, 3, 5, 7, 9, 11, 13}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({0, 12, 20, 25}), std::vector<uint64_t>({1, 13, 17, 21}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({4, 24}), std::vector<uint64_t>({5, 19}));
}

BOOST_AUTO_TEST_CASE(simple_remove)
{
    std::unique_ptr<Accumulator> full = MakeEmpty();

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);
    for (Leaf& leaf : leaves) {
        leaf.second = true;
    }
    BOOST_CHECK(full->Modify(leaves, {}));
    BOOST_CHECK(full->GetCachedLeaves().size() == 15);

    BOOST_CHECK(full->Modify({}, {14}));
    BOOST_CHECK(!full->IsCached(leaves[14].first));
}

void PrintLeaves(Accumulator& acc)
{
    std::vector<Hash> leaves{acc.GetCachedLeaves()};
    std::cout << "cached leaves: " << std::endl;
    for (const Hash& hash : leaves) {
        std::cout << HexStr(hash) << std::endl;
    }
}

void TestUndo(Accumulator& pruned, Accumulator& full, const std::vector<Leaf>& leaves, const std::vector<Hash>& deletions)
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

/*BOOST_AUTO_TEST_CASE(undo)
{
    std::unique_ptr<Accumulator> full = MakeEmpty();
    std::unique_ptr<Accumulator> pruned = MakeEmpty();

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
        std::unique_ptr<Accumulator> full = MakeEmpty();
        std::unique_ptr<Accumulator> pruned = MakeEmpty();

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

BOOST_AUTO_TEST_CASE(simple_modified_proof)
{
    std::unique_ptr<Accumulator> full = MakeEmpty();
    std::unique_ptr<Accumulator> pruned = MakeEmpty();

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Add test leaves, dont delete any.
    BOOST_CHECK(pruned->Modify(leaves, {}));
    for (Leaf& leaf : leaves) {
        leaf.second = true;
    }
    BOOST_CHECK(full->Modify(leaves, {}));

    BatchProof proof;
    BOOST_CHECK(full->Prove(proof, {leaves[0].first}));
    BOOST_CHECK(pruned->Verify(proof, {leaves[0].first}));

    std::vector<Hash> modified_hashes = proof.GetHashes();
    // Fill the last hash with zeros.
    // This should cause verification to fail.
    modified_hashes.back().fill(0);
    BatchProof invalid(proof.GetSortedTargets(), modified_hashes);
    // Assert that verification fails.
    BOOST_CHECK(!pruned->Verify(invalid, {leaves[0].first}));
}

BOOST_AUTO_TEST_SUITE_END()
