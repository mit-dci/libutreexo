#include "../../include/utreexo.h"
#include "../nodepool.h"
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <cstring>

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
    Accumulator* full = (Accumulator*)new Pollard(0, 160);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 64);

    BOOST_CHECK(full->Modify(leaves, {}));

    delete full;
}

BOOST_AUTO_TEST_CASE(simple_full)
{
    Accumulator* full = (Accumulator*)new RamForest(0, 32);

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
    Accumulator* full = (Accumulator*)new Pollard(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 32);
    // Remember all leaves
    for (Leaf& leaf : leaves)
        leaf.second = true;
    // Add test leaves, dont delete any.
    full->Modify(leaves, std::vector<uint64_t>());
    // Delete some leaves, dont add any new ones.
    leaves.clear();
    full->Modify(leaves, {0, 2, 3, 9});
    //full->PrintRoots();

    delete full;
}

BOOST_AUTO_TEST_CASE(batchproof_serialization)
{
    Accumulator* full = (Accumulator*)new RamForest(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 32);

    full->Modify(leaves, std::vector<uint64_t>());

    std::vector<uint8_t> proof_bytes;
    BatchProof proof1;
    full->Prove(proof1, {leaves[0].first, leaves[1].first});
    proof1.Serialize(proof_bytes);

    BatchProof proof2;
    BOOST_CHECK(proof2.Unserialize(proof_bytes));
    BOOST_CHECK(proof1 == proof2);
}

BOOST_AUTO_TEST_CASE(singular_leaf_prove)
{
    Pollard pruned(0, 64);
    RamForest full(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Add test leaves, dont delete any.
    full.Modify(leaves, {});
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
    Pollard pruned(0, 64);
    RamForest full(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Add test leaves, dont delete any.
    full.Modify(leaves, {});
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
    Pollard pruned(0, 64);
    RamForest full(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 8);

    // Remember leaf 0 in the pollard.
    leaves[0].second = true;
    // Add test leaves, dont delete any.
    full.Modify(leaves, {});
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
    Pollard pruned(0, 64);
    RamForest full(0, 64);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);

    // Add test leaves, dont delete any.
    full.Modify(leaves, {});
    pruned.Modify(leaves, {});
    //full.PrintRoots();

    BatchProof proof;
    full.Prove(proof, {leaves[0].first, leaves[7].first, leaves[8].first, leaves[14].first});

    BOOST_CHECK(pruned.Verify(proof, {leaves[0].first, leaves[7].first, leaves[8].first, leaves[14].first}));
}

BOOST_AUTO_TEST_CASE(simple_batchproof_verify_and_delete)
{
    RamForest full(0, 128);
    Pollard pruned(0, 128);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);

    full.Modify(leaves, {});
    pruned.Modify(leaves, {});

    // Check that the roots of the full forest match the pollard roots.
    std::vector<Hash> prev_full_roots, prev_pruned_roots;
    full.Roots(prev_full_roots);
    pruned.Roots(prev_pruned_roots);
    BOOST_CHECK(prev_full_roots == prev_pruned_roots);

    // Prove and verify some leaves.
    // This should populate the pollard with the required proof for deletion.
    BatchProof proof;
    BOOST_CHECK(full.Prove(proof, {leaves[0].first, leaves[7].first, leaves[8].first, leaves[14].first}));
    BOOST_CHECK(pruned.Verify(proof, {leaves[0].first, leaves[7].first, leaves[8].first, leaves[14].first}));

    // Delete the targets.
    BOOST_CHECK(full.Modify({}, proof.GetSortedTargets()));
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
    RamForest full(0, 128);
    Pollard pruned(0, 128);

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, 15);

    // Remember leaf 0
    leaves[0].second = true;

    full.Modify(leaves, {});
    pruned.Modify(leaves, {});

    // Prove and verify some leaves.
    // This should populate the pollard with the required proof for deletion.
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

    BOOST_CHECK(!pruned.Verify(BatchProof(proof.GetSortedTargets(), {invalid_hash}), leaf_hashes));
    BOOST_CHECK(pruned.Verify(proof, leaf_hashes));
}

BOOST_AUTO_TEST_SUITE_END()
