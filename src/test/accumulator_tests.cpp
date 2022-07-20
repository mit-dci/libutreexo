#include "utils.h"
#include "utreexo.h"
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <cstring>
#include <random>
#include <vector>
#include <iostream>

BOOST_AUTO_TEST_SUITE(accumulator_tests)

using namespace utreexo;

using Hash = std::array<uint8_t, 32>;
using Leaf = std::pair<Hash, bool>;
using BatchProof = BatchProof<Hash>;

static void PrintProof(const BatchProof& proof)
{
    std::cout << "===Proof===" << std::endl;
    for (const uint64_t& target : proof.GetSortedTargets()) {
        std::cout << target << ", ";
    }
    std::cout << std::endl;

    for (const Hash& hash : proof.GetHashes()) {
        std::cout << HexStr(hash).substr(0, 8) << ", ";
    }
    std::cout << std::endl;
    std::cout << "===========" << std::endl;
}

static void TestSerialization(const Accumulator<Hash>& full, const Accumulator<Hash>& pruned, const std::vector<Hash>& leaf_hashes)
{

    BatchProof proof;
    BOOST_CHECK(full.Prove(proof, leaf_hashes));

    std::vector<uint8_t> pruned_bytes, full_bytes;
    pruned.Serialize(pruned_bytes);
    full.Serialize(full_bytes);

    auto pruned_restored{MakeEmpty<Hash>()};
    pruned_restored->Unserialize(pruned_bytes);
    BOOST_CHECK(pruned_restored->Verify(proof, leaf_hashes));

    auto full_restored{MakeEmpty<Hash>()};
    full_restored->Unserialize(full_bytes);

    BatchProof proof_after_restore;
    BOOST_CHECK(full_restored->Prove(proof_after_restore, leaf_hashes));
    BOOST_CHECK(proof_after_restore == proof);
    BOOST_CHECK(pruned_restored->Verify(proof_after_restore, leaf_hashes));
}

BOOST_AUTO_TEST_CASE(serialize)
{
    auto pruned{MakeEmpty<Hash>()};
    auto full{MakeEmpty<Hash>()};

    // 1. Add 64 elements
    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 63);

    BOOST_CHECK(pruned->Modify(leaves, {}));
    MarkLeavesAsMemorable(leaves);
    BOOST_CHECK(full->Modify(leaves, {}));

    std::vector<Hash> leaf_hashes = {leaves[0].first, leaves[2].first, leaves[3].first, leaves[9].first};
    TestSerialization(*full, *pruned, leaf_hashes);
}

// Simple test that adds and deletes a bunch of elements from both a forest and pollard.
BOOST_AUTO_TEST_CASE(simple)
{
    auto pruned{MakeEmpty<Hash>()};
    auto full{MakeEmpty<Hash>()};

    // 1. Add 64 elements
    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 64);

    BOOST_CHECK(pruned->Modify(leaves, {}));
    MarkLeavesAsMemorable(leaves);
    BOOST_CHECK(full->Modify(leaves, {}));

    {
        // 2. Check that roots of the forest and pollard match
        const auto [pruned_root_bits, pruned_roots] = pruned->GetState();
        const auto [full_root_bits, full_roots] = full->GetState();

        BOOST_CHECK(pruned_root_bits == full_root_bits);
        BOOST_CHECK(pruned_roots.size() == 1);
        BOOST_CHECK(full_roots.size() == 1);

        BOOST_CHECK(pruned_roots[0] == HashFromStr("6692c043bfd19c717a07a931833b1748cff69aa4349110948907ab125f744c25"));
        BOOST_CHECK(pruned_roots == full_roots);
    }

    // 3. Prove elements 0, 2, 3 and 9 in the forest
    BatchProof proof;
    std::vector<Hash> leaf_hashes = {leaves[0].first, leaves[2].first, leaves[3].first, leaves[9].first};
    BOOST_CHECK(full->Prove(proof, leaf_hashes));

    // 4. Let the pollard verify the proof
    BOOST_CHECK(pruned->Verify(proof, leaf_hashes));

    // 5. Delete the elements from both the forest and pollard
    BOOST_CHECK(full->Modify({}, proof.GetSortedTargets()));
    BOOST_CHECK(pruned->Modify({}, proof.GetSortedTargets()));

    {
        // 6. Check that roots of the forest and pollard match
        const auto [pruned_root_bits, pruned_roots] = pruned->GetState();
        const auto [full_root_bits, full_roots] = full->GetState();

        BOOST_CHECK(pruned_root_bits == full_root_bits);
        BOOST_CHECK(pruned_roots.size() == 1);
        BOOST_CHECK(full_roots.size() == 1);

        BOOST_CHECK(pruned_roots[0] == HashFromStr("7608aecffadc9b82fd2482d0c32ef0389236ceaf25bf7287d2caa26ccd2716a5"));
        BOOST_CHECK(pruned_roots == full_roots);
    }
}

BOOST_AUTO_TEST_CASE(singular_leaf_prove)
{
    auto pruned{MakeEmpty<Hash>()};
    auto full{MakeEmpty<Hash>()};

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Add test leaves, dont delete any.
    BOOST_CHECK(pruned->Modify(leaves, {}));
    MarkLeavesAsMemorable(leaves);
    BOOST_CHECK(full->Modify(leaves, {}));

    for (Leaf& leaf : leaves) {
        BatchProof proof;
        BOOST_CHECK(full->Prove(proof, {leaf.first}));

        BatchProof proof_from_pruned;
        BOOST_CHECK(!pruned->Prove(proof, {leaf.first}));
        BOOST_CHECK(pruned->Verify(proof, {leaf.first}));
        BOOST_CHECK(pruned->Prove(proof_from_pruned, {leaf.first}));
        BOOST_CHECK(proof == proof_from_pruned);
        BOOST_CHECK(pruned->GetCachedLeaves().size() == 1);

        // Delete cached leaf.
        pruned->Uncache(leaf.first);
        BOOST_CHECK(!pruned->Prove(proof, {leaf.first}));
        BOOST_CHECK(pruned->GetCachedLeaves().size() == 0);
    }
}

BOOST_AUTO_TEST_CASE(simple_modified_proof)
{
    auto pruned{MakeEmpty<Hash>()};
    auto full{MakeEmpty<Hash>()};

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Add test leaves, dont delete any.
    BOOST_CHECK(pruned->Modify(leaves, {}));
    MarkLeavesAsMemorable(leaves);
    BOOST_CHECK(full->Modify(leaves, {}));

    BatchProof proof;
    BOOST_CHECK(full->Prove(proof, {leaves[0].first}));
    BOOST_CHECK(pruned->Verify(proof, {leaves[0].first}));
    pruned->Uncache(leaves[0].first);

    std::vector<Hash> modified_hashes{proof.GetHashes()};
    // Fill the last hash with zeros.
    // This should cause verification to fail.
    modified_hashes.back().fill(0);
    BatchProof invalid(proof.GetSortedTargets(), modified_hashes);
    // Assert that verification fails.
    BOOST_CHECK(!pruned->Verify(invalid, {leaves[0].first}));
}

BOOST_AUTO_TEST_CASE(simple_blockchain)
{
    auto pruned{MakeEmpty<Hash>()};
    auto full{MakeEmpty<Hash>()};

    // Simulate the addition of 1000 blocks with a uniformly distributed
    // number of additions and deletions in each block
    int num_blocks = 1000;
    int num_max_adds = 128;
    int num_max_dels = 128;
    int unique_hash = 0;

    std::default_random_engine generator;
    std::uniform_int_distribution<int> add_distribution(1, num_max_adds);

    // Add genesis leaves to have something to delete on first iteration
    {
        std::vector<Leaf> adds;
        CreateTestLeaves(adds, 8, unique_hash);
        unique_hash += adds.size();
        BOOST_CHECK(pruned->Modify(adds, {}));
        MarkLeavesAsMemorable(adds);
        BOOST_CHECK(full->Modify(adds, {}));
    }

    // TODO: Process blocks while keeping the UndoBatch on every step in order to
    // rollback all modifications in the end
    // std::vector<std::pair<UndoBatch, std::vector<Hash>>> undos;

    for (int i = 0; i < num_blocks; i++) {
        // Create leaves to-add
        int num_adds = add_distribution(generator);
        std::vector<Leaf> adds;
        CreateTestLeaves(adds, num_adds, unique_hash);
        unique_hash += adds.size();

        // Select leaves for deletion
        std::vector<Hash> cached_leaf_hashes{full->GetCachedLeaves()};
        std::vector<Hash> leaf_hashes;
        int min = 0, max = std::min(num_max_dels, (int)cached_leaf_hashes.size() - 1);
        while (max > 0 && min != max) {
            std::uniform_int_distribution<int> del_distribution(min, max);
            int del_index = del_distribution(generator);
            leaf_hashes.push_back(cached_leaf_hashes[del_index]);
            min = std::min(max, del_index + 1);
        }

        TestSerialization(*full, *pruned, leaf_hashes);

        BatchProof proof;
        BOOST_CHECK(full->Prove(proof, leaf_hashes));
        BOOST_CHECK(full->Modify(CopyLeavesAsMemorable(adds), proof.GetSortedTargets()));

        // TODO: Keep the UndoBatch with the roots it will rollback to
        // undos.emplace_back(undo, roots);

        // TODO Undo and redo last modification to test a rollback
        //BOOST_CHECK(full.Undo(undo));
        // full.Roots(roots); // roots after the rollback
        // roots after the rollback should match those kept with the UndoBatch
        // BOOST_CHECK(roots == undos.back().second);
        // BOOST_CHECK(full->Modify(CopyLeavesAsMemorable(adds), proof.GetSortedTargets()));

        // Verify the proof with pollard and modify pollard to new state
        BOOST_TEST(pruned->Verify(proof, leaf_hashes));

        // The pollard should be able to produce a proof for any of the cached leaves.
        {
            const auto [root_bits, roots] = pruned->GetState();
            auto pruned_copy{Make<Hash>(root_bits, roots)};

            for (const Hash& hash : leaf_hashes) {
                BatchProof leaf_proof;
                BOOST_CHECK(pruned->Prove(leaf_proof, {hash}));
                BOOST_CHECK(pruned_copy->Verify(leaf_proof, {hash}));
                pruned_copy->Uncache(hash);
            }
        }

        BOOST_CHECK(pruned->Modify(adds, proof.GetSortedTargets()));
        BOOST_CHECK(pruned->GetCachedLeaves().size() == 0);
        {
            const auto [pruned_root_bits, pruned_roots] = pruned->GetState();
            const auto [full_root_bits, full_roots] = full->GetState();

            BOOST_CHECK(pruned_root_bits == full_root_bits);
            BOOST_CHECK(pruned_roots == full_roots);
        }

        if (proof.GetTargets().size() > 0) BOOST_CHECK(!pruned->Verify(proof, leaf_hashes));
    }

    // std::vector<Hash> roots;
    // TODO Rollback all modifications and check that we arrive at the initial state.
    // int height = num_blocks - 1;
    // for (auto it = undos.crbegin(); it != undos.crend(); ++it) {
    //     const UndoBatch& undo = it->first;
    //     BOOST_CHECK(full.Undo(undo));
    //     roots.clear();
    //
    //     full.Roots(roots);
    //     BOOST_CHECK(roots == it->second);
    //     --height;
    // }
    // BOOST_CHECK(height == -1);
}

/*BOOST_AUTO_TEST_CASE(simple_cached_proof)
{
    Pollard pruned(0);
    RamForest full(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Remember leaf 0 in the pollard.
    leaves[0].second = true;
    // Add test leaves, dont delete any.
    full.Modify(unused_undo, leaves, {});
    pruned.Modify(leaves, {});
    // full.PrintRoots();

    BatchProof proof;
    full.Prove(proof, {leaves[0].first});

    // Since the proof for leaf 0 is cached,
    // the proof can be any subset of the full proof.
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetSortedTargets(), {proof.GetHashes()[0]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetSortedTargets(), {proof.GetHashes()[1]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetSortedTargets(), {proof.GetHashes()[2]}), {leaves[0].first}));

    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetSortedTargets(), {proof.GetHashes()[0], proof.GetHashes()[1]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetSortedTargets(), {proof.GetHashes()[0], proof.GetHashes()[2]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetSortedTargets(), {proof.GetHashes()[1], proof.GetHashes()[2]}), {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(proof, {leaves[0].first}));
    // Empty proof should work since the pollard now holds the computed nodes as well.
    BOOST_CHECK(pruned.Verify(BatchProof(proof.GetSortedTargets(), {}), {leaves[0].first}));
}

BOOST_AUTO_TEST_CASE(simple_batch_proof)
{
    Pollard pruned(0);
    RamForest full(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);

    // Add test leaves, dont delete any.
    full.Modify(unused_undo, leaves, {});
    pruned.Modify(leaves, {});
    // full.PrintRoots();

    BatchProof proof;
    full.Prove(proof, {leaves[0].first, leaves[7].first, leaves[8].first, leaves[14].first});

    BOOST_CHECK(pruned.Verify(proof, {leaves[0].first, leaves[7].first, leaves[8].first, leaves[14].first}));
}

BOOST_AUTO_TEST_CASE(simple_batchproof_verify_and_delete)
{
    RamForest full(0);
    Pollard pruned(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);

    full.Modify(unused_undo, leaves, {});
    pruned.Modify(leaves, {});

    // Check that the roots of the full forest match the pollard roots.
    std::vector<Hash> prev_full_roots, prev_pruned_roots;
    full.Roots(prev_full_roots);
    pruned.Roots(prev_pruned_roots);
    BOOST_CHECK(prev_full_roots == prev_pruned_roots);

    // Prove and verify some leaves.
    // This should populate the pollard with the required proof for deletion.
    BatchProof proof;
    // The order of hashes should be irrelevant for Prove
    BOOST_CHECK(full.Prove(proof, {leaves[7].first, leaves[8].first, leaves[14].first, leaves[0].first}));
    // The order of hashes should be relevant for Verify
    BOOST_CHECK(!pruned.Verify(proof, {leaves[7].first, leaves[8].first, leaves[14].first, leaves[0].first}));
    BOOST_CHECK(pruned.Verify(proof, {leaves[0].first, leaves[7].first, leaves[8].first, leaves[14].first}));

    // Deleting with out of order targets should cause modification to fail
    BOOST_CHECK(!full.Modify(unused_undo, {}, proof.GetTargets()));
    BOOST_CHECK(!pruned.Modify({}, proof.GetTargets()));

    // Delete the targets.
    BOOST_CHECK(full.Modify(unused_undo, {}, proof.GetSortedTargets()));
    BOOST_CHECK(pruned.Modify({}, proof.GetSortedTargets()));

    // Check that the roots of the full forest match the pollard roots.
    std::vector<Hash> full_roots, pruned_roots;
    full.Roots(full_roots);
    pruned.Roots(pruned_roots);
    BOOST_CHECK(full_roots == pruned_roots);

    // The new roots should be different than the previous ones.
    BOOST_CHECK(pruned_roots != prev_pruned_roots);
}

BOOST_AUTO_TEST_CASE(hash_to_known_invalid_proof)
{
    RamForest full(0);
    Pollard pruned(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);

    // Remember leaf 0
    leaves[0].second = true;

    full.Modify(unused_undo, leaves, {});
    pruned.Modify(leaves, {});
    BOOST_CHECK(pruned.NumCachedLeaves() == 1); // cached: 0

    BatchProof proof;
    std::vector<Hash> leaf_hashes = {leaves[4].first, leaves[5].first, leaves[6].first, leaves[7].first};
    BOOST_CHECK(full.Prove(proof, leaf_hashes));

    Hash invalid_hash;
    invalid_hash.fill(0xff);

    // Verification with an invalid proof hash should not pass.
    BOOST_CHECK(!pruned.Verify(BatchProof(proof.GetSortedTargets(), {invalid_hash}), leaf_hashes));
    BOOST_CHECK(pruned.Verify(proof, leaf_hashes));
    BOOST_CHECK(pruned.NumCachedLeaves() == 5); // cached: 0, 4, 5, 6, 7

    leaf_hashes = {leaves[1].first};
    BOOST_CHECK(full.Prove(proof, leaf_hashes));

    // Verification should fail if the number of targets specified in the proof
    // does not match the number of provided target hashes
    BOOST_CHECK(!pruned.Verify(BatchProof(proof.GetSortedTargets(), {invalid_hash}), leaf_hashes));
    BOOST_CHECK(pruned.Verify(proof, leaf_hashes));
    BOOST_CHECK(pruned.NumCachedLeaves() == 6); // cached: 0, 1, 4, 5, 6, 7
}

BOOST_AUTO_TEST_CASE(pollard_remember)
{
    RamForest full(0);
    Pollard pruned(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    leaves[1].second = true;
    leaves[2].second = true;
    leaves[3].second = true;

    BOOST_CHECK(full.Modify(unused_undo, leaves, {}));
    BOOST_CHECK(pruned.Modify(leaves, {}));

    BatchProof proof02;
    BOOST_CHECK(full.Prove(proof02, {leaves[0].first, leaves[2].first}));

    BOOST_CHECK(pruned.Verify(BatchProof({0, 2}, {leaves[1].first}), {leaves[0].first, leaves[2].first}));
    BOOST_CHECK(pruned.Verify(proof02, {leaves[0].first, leaves[2].first}));

    BOOST_CHECK(pruned.Modify({}, proof02.GetSortedTargets()));
    BOOST_CHECK(full.Modify(unused_undo, {}, proof02.GetSortedTargets()));

    // After the removal of 0 and 2, 3 should still be cached.
    BatchProof proof3;
    BOOST_CHECK(full.Prove(proof3, {leaves[3].first}));
    BOOST_CHECK(pruned.Modify({}, proof3.GetSortedTargets()));
    BOOST_CHECK(full.Modify(unused_undo, {}, proof3.GetSortedTargets()));

    // Check that the roots of the forest and the pollard are the same.
    std::vector<Hash> full_roots, pruned_roots;
    full.Roots(full_roots);
    pruned.Roots(pruned_roots);
    BOOST_CHECK(full_roots == pruned_roots);
}

BOOST_AUTO_TEST_CASE(simple_pollard_prove)
{
    RamForest full(0);
    Pollard pruned(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Set the deletion to 0.
    uint64_t target = 0;
    leaves[target].second = true;

    BOOST_CHECK(full.Modify(unused_undo, leaves, {}));
    BOOST_CHECK(pruned.Modify(leaves, {}));

    // Create the proofs.
    BatchProof full_proof;
    BatchProof pruned_proof;
    BOOST_CHECK(full.Prove(full_proof, {leaves[target].first}));
    BOOST_CHECK(pruned.Prove(pruned_proof, {leaves[target].first}));

    BOOST_CHECK(full_proof == pruned_proof);

    // Set the remember to false so that the proof isn't cached.
    leaves[target].second = false;

    // Finally check that the batchproof actually verifies.
    Pollard verify_pollard(0);
    verify_pollard.Modify(leaves, {});
    BOOST_CHECK(verify_pollard.Verify(pruned_proof, {leaves[target].first}));
}

BOOST_AUTO_TEST_CASE(ramforest_undo)
{
    // Two forests with one being on previous state to check against
    RamForest full(0), full_prev(0);
    int unique_hash = 0;

    // Create leaves to-add
    std::vector<Leaf> leaves, additional_leaves;
    CreateTestLeaves(leaves, 8, unique_hash);
    unique_hash += leaves.size();
    CreateTestLeaves(additional_leaves, 8, unique_hash);

    UndoBatch undo;
    BatchProof proof;
    std::vector<Hash> leaf_hashes;
    // ADD to state
    BOOST_CHECK(full.Modify(undo, leaves, {}));

    // Undo addition and then rollback
    BOOST_CHECK(full.Undo(undo));
    BOOST_CHECK(full == full_prev);
    BOOST_CHECK(full.Modify(unused_undo, leaves, {}));
    BOOST_CHECK(full_prev.Modify(unused_undo, leaves, {}));

    // REMOVE from state
    leaf_hashes = {leaves[5].first, leaves[6].first, leaves[7].first};
    BOOST_CHECK(full.Prove(proof, leaf_hashes));

    BOOST_CHECK(full.Modify(undo, {}, proof.GetSortedTargets()));
    BOOST_CHECK(!(full == full_prev));
    // Undo deletion and then rollback
    BOOST_CHECK(full.Undo(undo));
    BOOST_CHECK(full == full_prev);
    BOOST_CHECK(full.Modify(unused_undo, {}, proof.GetSortedTargets()));
    BOOST_CHECK(full_prev.Modify(unused_undo, {}, proof.GetSortedTargets()));

    // ADD & REMOVE to state
    leaf_hashes = {leaves[2].first, leaves[4].first};
    BOOST_CHECK(full.Prove(proof, leaf_hashes));
    BOOST_CHECK(full.Modify(undo, additional_leaves, proof.GetSortedTargets()));
    BOOST_CHECK(!(full == full_prev));
    // Undo modification
    BOOST_CHECK(full.Undo(undo));
    BOOST_CHECK(full == full_prev);
}

BOOST_AUTO_TEST_CASE(simple_posmap_updates)
{
    RamForest full(0);
    Pollard pruned(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 16);

    leaves[0].second = true;
    leaves[7].second = true;

    BOOST_CHECK(full.Modify(unused_undo, leaves, {}));
    BOOST_CHECK(pruned.Modify(leaves, {}));

    BOOST_CHECK(pruned.CountNodes() == 10);
    BOOST_CHECK(pruned.ComparePositionMap(full));
    BOOST_CHECK(pruned.NumCachedLeaves() == 2);

    BatchProof proof;
    BOOST_CHECK(pruned.Prove(proof, {leaves[0].first}));
    BOOST_CHECK(pruned.Verify(proof, {leaves[0].first}));

    BOOST_CHECK(pruned.Modify({}, {0}));
    BOOST_CHECK(full.Modify(unused_undo, {}, {0}));
    BOOST_CHECK(pruned.ComparePositionMap(full));
}

BOOST_AUTO_TEST_CASE(add_memorable_and_remove)
{
    RamForest full(0);
    Pollard pruned(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    leaves[0].second = true;

    BOOST_CHECK(full.Modify(unused_undo, leaves, {}));
    BOOST_CHECK(pruned.Modify(leaves, {}));

    BatchProof proof;
    BOOST_CHECK(pruned.Prove(proof, {leaves[0].first}));
    BOOST_CHECK(pruned.NumCachedLeaves() == 1);

    BOOST_CHECK(pruned.Modify({}, {0}));
}*/

BOOST_AUTO_TEST_SUITE_END()
