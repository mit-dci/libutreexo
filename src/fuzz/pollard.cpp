#include "pollard.h"
#include "batchproof.h"
#include "ram_forest.h"
#include "fuzz.h"
#include <iostream>

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

FUZZ(pollard)
{
    RamForest forest(0);
    Pollard pollard(0);

    FUZZ_CONSUME(uint8_t, num_leaves);
    num_leaves = num_leaves & 127;

    std::vector<Leaf> leaves;
    for (int i = 0; i < static_cast<int>(num_leaves); ++i) {
        Hash leaf_hash;
        SetHash(leaf_hash, i);
        FUZZ_CONSUME_BOOL(remember);
        leaves.emplace_back(leaf_hash, remember);
    }

    {
        UndoBatch undo;
        assert(forest.Modify(undo, leaves, {}));
        assert(pollard.Modify(leaves, {}));
    }

    std::vector<Hash> targets;
    for (int i = 0; i < static_cast<int>(num_leaves); ++i) {
        FUZZ_CONSUME_BOOL(remove);
        if (remove) {
            targets.push_back(forest.GetLeaf(i));
        }
    }

    // Let the forest prove the target leaves.
    BatchProof proof;
    assert(forest.Prove(proof, targets));

    // Verify the proof.
    FUZZ_CONSUME_BOOL(proof_ok);
    if (proof.GetHashes().size() > 0 && !proof_ok) {
        // Invalidate the proof by changing one hash
        std::vector<Hash> hashes = proof.GetHashes();
        hashes[0].fill(0);
        assert(!pollard.Verify(BatchProof{proof.GetTargets(), hashes}, targets));
        return;
    } else {
        assert(pollard.Verify(proof, targets));
    }

    // Remove the targets from both the forest and the pollard.
    {
        UndoBatch undo;
        assert(forest.Modify(undo, {}, proof.GetSortedTargets()));
        assert(pollard.Modify({}, proof.GetSortedTargets()));
        std::vector<Hash> forest_roots, pollard_roots;
        forest.Roots(forest_roots);
        pollard.Roots(pollard_roots);
        assert(forest_roots == pollard_roots);
    }
}
