#ifndef UTREEXO_ACCUMULATOR_H
#define UTREEXO_ACCUMULATOR_H

#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <stdint.h>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace utreexo {
class ForestState;

using Hash = std::array<uint8_t, 32>;
using Leaf = std::pair<Hash, bool>;
template <typename T>
using NodePtr = std::shared_ptr<T>;

class BatchProof;

/** Provides an interface for a hash based dynamic accumulator. */
class Accumulator
{
public:
    Accumulator(uint64_t num_leaves);
    virtual ~Accumulator();

    /**
     * Verify a batch proof.
     * Return whether or not the proof proved the target hashes.
     * The internal state of the accumulator might be mutated but the roots will not.
     */
    virtual bool Verify(const BatchProof& proof, const std::vector<Hash>& target_hashes) = 0;

    /** Modify the accumulator by adding leaves and removing targets. */
    bool Modify(const std::vector<Leaf>& new_leaves, const std::vector<uint64_t>& targets);

    /**
     * Create a batch proof for a set of target hashes. (A target hash is the hash a leaf in the forest)
     * The target hashes are not required to be sorted by leaf position in the forest and
     * the targets of the batch proof will have the same order as the hashes.
     *
     * Example:
     *   target_hashes = [hash of leaf 50, hash of leaf 10, hash of leaf 20]
     *   proof.targets = [50, 10, 20]
     *
     * Return true on success and false on failure. Proving can fail if a target hash does not
     * exist in the accumulator. For forests, this failure will only happen if a target hash
     * doesn't exist at all. For Pollards, a target hash could exist but the failure may still
     * happen as Pollard doesn't cache every hash.
     */
    bool Prove(BatchProof& proof, const std::vector<Hash>& target_hashes) const;

    /** Return the root hashes (roots of taller trees first) */
    void Roots(std::vector<Hash>& roots) const;

    bool ComparePositionMap(Accumulator& other) const;
    void PrintPositionMap() const;
    void PrintRoots() const;

    uint64_t NumLeaves() const;

protected:
    struct LeafHasher {
        size_t operator()(const Hash& hash) const;
    };

    /*
     * Node represents a node in the accumulator forest.
     * This is used to create an abstraction on top of a accumulator implementation,
     * because it might not use a pointer based tree datastructure but the verification and modification
     * algorithms are quite nicely expressed using one.
     */
    class Node;

    // The number of leaves in the forest.
    uint64_t m_num_leaves;

    // The roots of the accumulator.
    std::vector<NodePtr<Accumulator::Node>> m_roots;

    // A map from leaf hashes to their positions.
    // This is needed for proving that leaves are included in the accumulator.
    // The forest will always have all the positions of all the leaves. The pollard
    // has the option of pruning leaves, thus will not always have all the positions
    // of all the leaves.
    std::unordered_map<Hash, uint64_t, LeafHasher> m_posmap;

    void UpdatePositionMapForRange(uint64_t from, uint64_t to, uint64_t range);
    void UpdatePositionMapForSubtreeSwap(uint64_t from, uint64_t to);

    /* Return the hash at a position */
    virtual std::optional<const Hash> Read(uint64_t pos) const = 0;
    /* Return all hashes that are available in the interval [pos, pos+range[ */
    virtual std::vector<Hash> ReadLeafRange(uint64_t pos, uint64_t range) const = 0;

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
    virtual bool Add(const std::vector<Leaf>& leaves);
    /* Remove target leaves from the accumulator. */
    bool Remove(const std::vector<uint64_t>& targets);

    /* Compute the parent hash from two children. */
    static void ParentHash(Hash& parent, const Hash& left, const Hash& right);

    template <class T, typename... Args>
    static NodePtr<T> MakeNodePtr(const Args&... args)
    {
        NodePtr<T> node = std::make_shared<T>(args...);
        if (!node) {
            throw std::runtime_error("Accumulator::MakeNodePtr failed to allocate node.");
        }

        return node;
    }
};

};     // namespace utreexo
#endif // UTREEXO_ACCUMULATOR_H
