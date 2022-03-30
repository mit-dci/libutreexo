#include "../../include/utreexo.h"
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <cstring>
#include <random>

#include "state.h"

BOOST_AUTO_TEST_SUITE(accumulator_tests)

using namespace utreexo;

void SetHash(Hash& hash, int num)
{
    hash[0] = num;
    hash[1] = num >> 8;
    hash[2] = num >> 16;
    hash[3] = num >> 24;
    hash[4] = 0xFF;
}

void CreateTestLeaves(std::vector<Leaf>& leaves, int count, int offset)
{
    for (int i = 0; i < count; i++) {
        Hash hash = {}; // initialize all elements to 0
        SetHash(hash, offset + i);
        leaves.emplace_back(std::move(hash), false);
    }
}

void CreateTestLeaves(std::vector<Leaf>& leaves, int count)
{
    CreateTestLeaves(leaves, count, 0);
}

Hash HashFromStr(const std::string& hex)
{
    const signed char p_util_hexdigit[256] =
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,
         -1, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    Hash h;
    assert(hex.size() == 64);
    int digits = 64;

    for (int i = 31; i >= 0;) {
        h[i] = p_util_hexdigit[hex[--digits]];
        if (digits > 0) {
            h[i] |= p_util_hexdigit[hex[--digits]] << 4;
            i--;
        }
    }

    return h;
}

// https://github.com/bitcoin/bitcoin/blob/7f653c3b22f0a5267822eec017aea6a16752c597/src/util/strencodings.cpp#L580
template <class T>
std::string HexStr(const T s)
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

utreexo::UndoBatch unused_undo;

// Simple test that adds and deletes a bunch of elements from both a forest and pollard.
BOOST_AUTO_TEST_CASE(simple)
{
    Pollard pruned(0);
    RamForest full(0);

    // 1. Add 64 elements
    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 64);

    BOOST_CHECK(pruned.Modify(leaves, {}));
    BOOST_CHECK(full.Modify(unused_undo, leaves, {}));

    // 2. Check that roots of the forest and pollard match
    std::vector<Hash> pruned_roots, full_roots;
    pruned.Roots(pruned_roots);
    full.Roots(full_roots);

    BOOST_CHECK(pruned_roots.size() == 1);
    BOOST_CHECK(full_roots.size() == 1);

    BOOST_CHECK(pruned_roots[0] == HashFromStr("6692c043bfd19c717a07a931833b1748cff69aa4349110948907ab125f744c25"));
    BOOST_CHECK(pruned_roots == full_roots);

    // 3. Prove elements 0, 2, 3 and 9 in the forest
    BatchProof proof;
    std::vector<Hash> leaf_hashes = {leaves[0].first, leaves[2].first, leaves[3].first, leaves[9].first};
    BOOST_CHECK(full.Prove(proof, leaf_hashes));

    // 4. Let the pollard verify the proof
    BOOST_CHECK(pruned.Verify(proof, leaf_hashes));

    // 5. Delete the elements from both the forest and pollard
    utreexo::UndoBatch undo;
    BOOST_CHECK(full.Modify(undo, {}, proof.GetSortedTargets()));
    BOOST_CHECK(pruned.Modify({}, proof.GetSortedTargets()));

    // 6. Check that the roots match after the deletion
    pruned.Roots(pruned_roots);
    full.Roots(full_roots);

    BOOST_CHECK(pruned_roots.size() == 4);
    BOOST_CHECK(full_roots.size() == 4);

    BOOST_CHECK(pruned_roots[0] == HashFromStr("b868b67e97610dc20fd4052d08c390cf8fc95eef3c7ee717aebc85a02e81cd68"));
    BOOST_CHECK(pruned_roots[1] == HashFromStr("4a812f9dc3a1691f4b6de9ec07ecb2014a3c8839182592549a2d8508631cd908"));
    BOOST_CHECK(pruned_roots[2] == HashFromStr("891899fa84a5c8659b007ce655b7cc5cf4b92493477db1095518cfc732024ef2"));
    BOOST_CHECK(pruned_roots[3] == HashFromStr("aee875faf7276a9817d0db6195414118b1348697d2e2abd4b3fcee46c579833b"));
    BOOST_CHECK(pruned_roots == full_roots);

    std::vector<uint8_t> undo_bytes;
    undo.Serialize(undo_bytes);
    {
        UndoBatch copy;
        BOOST_CHECK(copy.Unserialize(undo_bytes));
        BOOST_CHECK(copy == undo);
    }
    // Undo last modification
    BOOST_CHECK(full.Undo(undo));

    full.Roots(full_roots);
    BOOST_CHECK(full_roots.size() == 1);
    BOOST_CHECK(full_roots[0] == HashFromStr("6692c043bfd19c717a07a931833b1748cff69aa4349110948907ab125f744c25"));
}

BOOST_AUTO_TEST_CASE(ramforest_disk)
{
    std::remove("./test_forest");
    BatchProof proof;
    std::vector<Leaf> leaves;
    {
        RamForest full("./test_forest");
        Pollard pollard(0);

        CreateTestLeaves(leaves, 32);

        BOOST_CHECK(full.Modify(unused_undo, leaves, {}));
        BOOST_CHECK(pollard.Modify(leaves, {}));
        BOOST_CHECK(full.Prove(proof, {leaves[0].first}));
        BOOST_CHECK(pollard.Verify(proof, {leaves[0].first}));
    }

    RamForest full("./test_forest");
    BatchProof copy;
    BOOST_CHECK(full.Prove(copy, {leaves[0].first}));
    BOOST_CHECK(copy == proof);
}


BOOST_AUTO_TEST_CASE(batchproof_serialization)
{
    RamForest full(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 32);

    full.Modify(unused_undo, leaves, {});

    std::vector<uint8_t> proof_bytes;
    BatchProof proof1;
    BOOST_CHECK(full.Prove(proof1, {leaves[0].first, leaves[1].first}));
    proof1.Serialize(proof_bytes);

    BatchProof proof2;
    BOOST_CHECK(proof2.Unserialize(proof_bytes));
    BOOST_CHECK(proof1 == proof2);
}

BOOST_AUTO_TEST_CASE(singular_leaf_prove)
{
    Pollard pruned(0);
    RamForest full(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Add test leaves, dont delete any.
    full.Modify(unused_undo, leaves, {});
    pruned.Modify(leaves, {});
    //full.PrintRoots();

    for (Leaf& leaf : leaves) {
        BatchProof proof;
        full.Prove(proof, {leaf.first});
        BOOST_CHECK(pruned.Verify(proof, {leaf.first}));

        // Delete all cached leaves.
        pruned.Prune();
    }
}

BOOST_AUTO_TEST_CASE(simple_modified_proof)
{
    Pollard pruned(0);
    RamForest full(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Add test leaves, dont delete any.
    full.Modify(unused_undo, leaves, {});
    pruned.Modify(leaves, {});
    // full.PrintRoots();

    BatchProof proof;
    full.Prove(proof, {leaves[0].first});
    std::vector<Hash> modified_hashes = proof.GetHashes();
    // Fill the last hash with zeros.
    // This should cause verification to fail.
    modified_hashes.back().fill(0);
    BatchProof invalid(proof.GetSortedTargets(), modified_hashes);
    // Assert that verification fails.
    BOOST_CHECK(pruned.Verify(invalid, {leaves[0].first}) == false);
}

BOOST_AUTO_TEST_CASE(simple_cached_proof)
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
    //full.PrintRoots();

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
    //full.PrintRoots();

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

    BatchProof proof;
    std::vector<Hash> leaf_hashes = {leaves[4].first, leaves[5].first, leaves[6].first, leaves[7].first};
    BOOST_CHECK(full.Prove(proof, leaf_hashes));

    Hash invalid_hash;
    invalid_hash.fill(0xff);

    // Verification with an invalid proof hash should not pass.
    BOOST_CHECK(!pruned.Verify(BatchProof(proof.GetSortedTargets(), {invalid_hash}), leaf_hashes));
    BOOST_CHECK(pruned.Verify(proof, leaf_hashes));

    leaf_hashes = {leaves[1].first};
    BOOST_CHECK(full.Prove(proof, leaf_hashes));

    // Verification should fail if the number of targets specified in the proof
    // does not match the number of provided target hashes
    BOOST_CHECK(!pruned.Verify(BatchProof(proof.GetSortedTargets(), {invalid_hash}), leaf_hashes));
    BOOST_CHECK(pruned.Verify(proof, leaf_hashes));
}

BOOST_AUTO_TEST_CASE(simple_blockchain)
{
    RamForest full(0);
    Pollard pruned(0);

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
        full.Modify(unused_undo, adds, {});
        pruned.Modify(adds, {});
    }

    // Process blocks while keeping the UndoBatch on every step in order to
    // rollback all modifications in the end
    std::vector<std::pair<UndoBatch, std::vector<Hash>>> undos;
    for (int i = 0; i < num_blocks; i++) {
        // Create leaves to-add
        int num_adds = add_distribution(generator);
        std::vector<Leaf> adds;
        CreateTestLeaves(adds, num_adds, unique_hash);
        unique_hash += adds.size();

        // Select leaves for deletion
        std::vector<Hash> leaf_hashes;
        int min = 0, max = full.NumLeaves() - 1;
        while (max > 0 && min != max) {
            std::uniform_int_distribution<int> del_distribution(min, max);
            int del_index = del_distribution(generator);
            leaf_hashes.push_back(full.GetLeaf(del_index));
            min = del_index + 1 > max ? max : del_index + 1;
        }

        std::vector<Hash> roots;
        full.Roots(roots); // roots before the modification

        BatchProof proof;
        UndoBatch undo;
        BOOST_CHECK(full.Prove(proof, leaf_hashes));
        BOOST_CHECK(full.Modify(undo, adds, proof.GetSortedTargets()));

        // Keep the UndoBatch with the roots it will rollback to
        undos.emplace_back(undo, roots);

        // Undo and redo last modification to test a rollback
        BOOST_CHECK(full.Undo(undo));
        full.Roots(roots); // roots after the rollback
        // roots after the rollback should match those kept with the UndoBatch
        BOOST_CHECK(roots == undos.back().second);

        BOOST_CHECK(full.Modify(unused_undo, adds, proof.GetSortedTargets()));

        // Verify the proof with pollard and modify pollard to new state
        BOOST_TEST(pruned.Verify(proof, leaf_hashes));
        BOOST_CHECK(pruned.Modify(adds, proof.GetSortedTargets()));

        if (proof.GetTargets().size() > 0) BOOST_CHECK(!pruned.Verify(proof, leaf_hashes));

        // check that roots match after modification
        std::vector<Hash> pruned_roots, full_roots;
        pruned.Roots(pruned_roots);
        full.Roots(full_roots);
        BOOST_CHECK(full_roots == pruned_roots);
    }


    std::vector<Hash> roots;
    // Rollback all modifications and check that we arrive at the initial state.
    int height = num_blocks - 1;
    for (auto it = undos.crbegin(); it != undos.crend(); ++it) {
        const UndoBatch& undo = it->first;
        BOOST_CHECK(full.Undo(undo));
        roots.clear();

        full.Roots(roots);
        BOOST_CHECK(roots == it->second);
        --height;
    }
    BOOST_CHECK(height == -1);
}

BOOST_AUTO_TEST_CASE(pollard_restore)
{
    RamForest full(0);
    Pollard pruned(0);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);

    full.Modify(unused_undo, leaves, {});
    pruned.Modify(leaves, {});

    BatchProof proof;
    std::vector<Hash> leaf_hashes = {leaves[4].first, leaves[5].first, leaves[6].first, leaves[7].first};
    BOOST_CHECK(full.Prove(proof, leaf_hashes));
    BOOST_CHECK(pruned.Verify(proof, leaf_hashes));

    std::vector<Hash> roots;
    pruned.Roots(roots);

    Pollard restored(roots, 15);
    BOOST_CHECK(restored.Verify(proof, leaf_hashes));
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

BOOST_AUTO_TEST_SUITE_END()
