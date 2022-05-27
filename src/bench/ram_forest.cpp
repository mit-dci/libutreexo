#include "bench.h"
#include "include/utreexo.h"
#include "util/leaves.h"

#include <vector>

using namespace utreexo;

static void AddElements(benchmark::Bench& bench, bool with_modify = false)
{
    UndoBatch unused_undo;
    const int num_leaves = bench.complexityN() > 1 ? static_cast<int>(bench.complexityN()) : 64;

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, num_leaves);

    // Benchmark
    if (with_modify) {
        bench.run([&]() {
            RamForest full(0);                    // initialize
            full.Modify(unused_undo, leaves, {}); // add leaves
        });
    } else {
        bench.run([&]() {
            RamForest full(0); // initialize
            full.Add(leaves);  // add leaves
        });
    }
}

// Benchmarks the creation of leaves
static void AddElementsForest(benchmark::Bench& bench)
{
    AddElements(bench);
}

// Benchmarks the creation of leaves using Modify
static void AddElementsWithModifyForest(benchmark::Bench& bench)
{
    AddElements(bench, true);
}

// Benchmarks the restoration from disk
static void RestoreFromDiskForest(benchmark::Bench& bench)
{
    UndoBatch unused_undo;
    const int num_leaves = bench.complexityN() > 1 ? static_cast<int>(bench.complexityN()) : 64;

    std::remove("./bench_forest"); // in case file already exists from prev run
    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, num_leaves);
    {
        RamForest full("./bench_forest");     // initialize
        full.Modify(unused_undo, leaves, {}); // add leaves
    }

    bench.run([&]() {
        RamForest full("./bench_forest");
        assert(full.NumLeaves() == num_leaves);
    });
}

// Benchmarks the proof of half the number of created leaves
static void ProveElementsForest(benchmark::Bench& bench)
{
    const int num_leaves_to_proof = bench.complexityN() > 1 ? static_cast<int>(bench.complexityN()) : 32;
    const int num_leaves = num_leaves_to_proof * 2;
    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, num_leaves);

    UndoBatch unused_undo;
    BatchProof proof;
    RamForest full(0); // initialize

    full.Modify(unused_undo, leaves, {}); // add leaves
    // select leaves to proof
    std::vector<Hash> leaf_hashes;
    random_unique(leaves.begin(), leaves.end(), num_leaves_to_proof);
    for (int i = 0; i < num_leaves_to_proof; ++i) {
        leaf_hashes.push_back(leaves[i].first);
    }

    // Benchmark
    bench.unit("proof").run([&] {
        full.Prove(proof, leaf_hashes);
    });
}

// Benchmarks the proof verification of half the number of created leaves
static void VerifyElementsForest(benchmark::Bench& bench)
{
    const int num_leaves_to_verify = bench.complexityN() > 1 ? static_cast<int>(bench.complexityN()) : 32;
    int num_leaves = num_leaves_to_verify * 2;
    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, num_leaves);
    UndoBatch unused_undo;
    BatchProof proof;
    RamForest full(0);

    full.Modify(unused_undo, leaves, {}); // add leaves to forest
    // select leaves to proof/verify
    std::vector<Hash> leaf_hashes;
    random_unique(leaves.begin(), leaves.end(), num_leaves_to_verify);
    for (int i = 0; i < num_leaves_to_verify; ++i) {
        leaf_hashes.push_back(leaves[i].first);
    }
    full.Prove(proof, leaf_hashes); // prove elements in the forest

    // Benchmark
    bench.unit("verification").run([&] {
        full.Verify(proof, leaf_hashes);
    });
}

// Benchmarks the removal of half the number of created leaves
// this benchmark unavoidably includes the creation of the leaves
static void RemoveElementsForest(benchmark::Bench& bench)
{
    UndoBatch unused_undo;
    BatchProof proof;
    const int num_leaves_to_remove = bench.complexityN() > 1 ? static_cast<int>(bench.complexityN()) : 32;
    const int num_leaves = num_leaves_to_remove * 2;

    std::vector<Leaf> leaves;
    CreateTestLeaves(leaves, num_leaves);

    // select leaves to remove
    std::vector<Hash> leaf_hashes;
    std::vector<Leaf> leaves_to_shuffle(leaves); // copy leaves
    random_unique(leaves_to_shuffle.begin(), leaves_to_shuffle.end(), num_leaves_to_remove);
    for (int i = 0; i < num_leaves_to_remove; ++i) {
        leaf_hashes.push_back(leaves_to_shuffle[i].first);
    }
    // create the forest once outside the benchmark to calculate the proof
    // without including the proof operation in the benchmark
    {
        RamForest full(0);
        full.Add(leaves);
        full.Prove(proof, leaf_hashes);
    }

    // Benchmark
    bench.run([&]() {
        // add leaves
        RamForest full(0);
        full.Add(leaves);

        // remove leaves
        full.Modify(unused_undo, {}, proof.GetSortedTargets());
    });
}

BENCHMARK(AddElementsForest);
BENCHMARK(AddElementsWithModifyForest);
BENCHMARK(RestoreFromDiskForest);
BENCHMARK(ProveElementsForest);
BENCHMARK(VerifyElementsForest);
BENCHMARK(RemoveElementsForest);