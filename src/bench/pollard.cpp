#include "bench.h"
#include "include/utreexo.h"
#include "util/leaves.h"

#include <vector>

using namespace utreexo;

// Benchmarks the creation of leaves
static void AddElementsPollard(benchmark::Bench& bench)
{
    const int num_leaves = bench.complexityN() > 1 ? static_cast<int>(bench.complexityN()) : 64;

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, num_leaves);

    // Benchmark
    bench.run([&]() {
        Pollard pruned(0);
        pruned.Modify(leaves, {});
    });
}

// Benchmark the proof verification of half the number of created leaves
// proof is first calculated from the forest
static void VerifyElementsPollard(benchmark::Bench& bench)
{
    const int num_leaves_to_verify = bench.complexityN() > 1 ? static_cast<int>(bench.complexityN()) : 32;
    int num_leaves = num_leaves_to_verify * 2;

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, num_leaves);

    UndoBatch unused_undo;
    BatchProof proof;
    Pollard pruned(0);
    RamForest full(0);

    full.Modify(unused_undo, leaves, {}); // add leaves to forest
    pruned.Modify(leaves, {});            // add leaves to pollard
    // select leaves to proof/verify
    std::vector<Hash> leaf_hashes;
    random_unique(leaves.begin(), leaves.end(), num_leaves_to_verify);
    for (int i = 0; i < num_leaves_to_verify; ++i) {
        leaf_hashes.push_back(leaves[i].first);
    }
    full.Prove(proof, leaf_hashes); // prove elements in the forest

    // Benchmark
    bench.unit("verification").run([&] {
        pruned.Verify(proof, leaf_hashes);
    });
}

// Benchmarks the restoration from roots
static void RestorePollard(benchmark::Bench& bench)
{
    const int num_leaves = bench.complexityN() > 1 ? static_cast<int>(bench.complexityN()) : 64;

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, num_leaves);

    // create pollard
    Pollard pruned(0);
    pruned.Modify(leaves, {});

    // get roots
    std::vector<Hash> roots;
    pruned.Roots(roots);

    // Benchmark
    bench.run([&]() {
        // restore pollard
        Pollard restored(roots, num_leaves);
    });
}

BENCHMARK(AddElementsPollard);
BENCHMARK(VerifyElementsPollard);
BENCHMARK(RestorePollard);