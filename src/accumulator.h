#ifndef UTREEXO_ACCUMULATOR_H
#define UTREEXO_ACCUMULATOR_H

#include <array>
#include <utility>
#include <vector>

namespace utreexo {
using Hash = std::array<uint8_t, 32>;
using Leaf = std::pair<Hash, bool>;

class BatchProof;

/** Provides an interface for a hash based dynamic accumulator. */
class Accumulator
{
public:
    Accumulator(uint64_t num_leaves);
    virtual ~Accumulator();

    /** 
     * Try to prove the provided targets. 
     * Return true on success and false on failure.
     */
    virtual bool Prove(BatchProof& proof, const std::vector<Hash>& target_hashes) const = 0;

    /**
     * TODO: Verify a proof.
     * The internal state of the accumulator might be mutated but the roots will not.
     * Return whether or not the proof proved the targetHashes.
     */
    /*bool Verify(const BatchProof& proof, const std::vector<Hash>& targetHashes);*/

    /** Modify the accumulator by adding leaves and removing targets. */
    bool Modify(const std::vector<Leaf>& new_leaves, const std::vector<uint64_t>& targets);

    /** Return the root hashes (roots of taller trees first) */
    void Roots(std::vector<Hash>& roots) const;

    void PrintRoots() const;

protected:
    /*
     * Node represents a node in the accumulator forest.
     * This is used to create an abstraction on top of a accumulator implementation,
     * because it might not use a pointer based tree datastructure but the verification and modification
     * algorithms are quite nicely expressed using one.
     */
    class Node;

    template <class T>
    class NodePool;
    template <class T>
    class NodePtr;

    // The number of leaves in the forest.
    uint64_t m_num_leaves;

    // The roots of the accumulator.
    std::vector<NodePtr<Accumulator::Node>> m_roots;

    /*
     * Swap two subtrees in the forest.
     * Return the nodes that need to be rehashed.
     */
    virtual NodePtr<Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) = 0;

    // MergeRoot and NewLeaf only have the desired effect if called correctly.
    // newLeaf should be called to allocate a new leaf.
    // After calling newLeaf, mergeRoot should be called for every consecutive least significant bit that is set to 1.

    /* Return the result of the latest merge. */
    virtual NodePtr<Accumulator::Node> MergeRoot(uint64_t parent_pos, Hash parent_hash) = 0;
    /* Allocate a new leaf and assign it the given hash */
    virtual NodePtr<Accumulator::Node> NewLeaf(const Leaf& leaf) = 0;

    /* Free memory or select new roots. */
    virtual void FinalizeRemove(uint64_t next_num_leaves) = 0;

    /* Add new leaves to the accumulator. */
    virtual void Add(const std::vector<Leaf>& leaves);
    /* Remove target leaves from the accumulator. */
    bool Remove(const std::vector<uint64_t>& targets);

    /* Compute the parent hash from to children. */
    static void ParentHash(Hash& parent, const Hash& left, const Hash& right);
};

};     // namespace utreexo
#endif // UTREEXO_ACCUMULATOR_H
