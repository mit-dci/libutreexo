#ifndef UTREEXO_ACCUMULATOR_H
#define UTREEXO_ACCUMULATOR_H

#include <memory>
#include <state.h>
#include <uint256.h>
#include <vector>

/**
 * Provides an interface for a hash based dynamic accumulator.
 *
 */
class Accumulator
{
public:
    /**
     * Provides and interface for data that can be added to the accumulator.
     */
    class Leaf
    {
    public:
        virtual uint256 Hash() const = 0;
        virtual bool Remember() const = 0;

        virtual ~Leaf() {}
    };

    class BatchProof
    {
    private:
        const std::vector<uint64_t> targets;
        std::vector<uint256> proof;

    public:
        BatchProof(const std::vector<uint64_t> targets, std::vector<uint256> proof)
            : targets(targets), proof(proof) {}

        bool Verify(ForestState state, const std::vector<uint256> roots, const std::vector<uint256> targetHashes) const;
        void Print();
    };

    Accumulator(ForestState& state) : m_state(state)
    {
        this->m_roots.reserve(64);
    }

    virtual ~Accumulator() {}
    virtual const BatchProof Prove(const std::vector<uint64_t>& targets) const = 0;

    bool Verify(const BatchProof& proof);
    void Modify(const std::vector<std::shared_ptr<Accumulator::Leaf>>& leaves,
                const std::vector<uint64_t>& targets);
    const std::vector<uint256> Roots() const;

    static uint256 ParentHash(const uint256& left, const uint256& right);

protected:
    class Node
    {
    protected:
        const ForestState& m_forest_state;

        //! Store the parent.
        //! This is useful if you want to rehash a path from the bottom up.
        std::shared_ptr<Accumulator::Node> m_parent;

        Node(const ForestState& state,
             uint64_t position)
            : m_forest_state(state), m_parent(nullptr), m_position(position) {}

        Node(const ForestState& state,
             uint64_t position,
             std::shared_ptr<Accumulator::Node> parent)
            : m_forest_state(state), m_parent(parent), m_position(position) {}

    public:
        //! The position of the node in the forest.
        uint64_t m_position;
        virtual ~Node() {}

        virtual const uint256& Hash() const = 0;
        virtual void ReHash() = 0;

        virtual std::shared_ptr<Accumulator::Node> Parent() const
        {
            return this->m_parent;
        }
    };

    ForestState& m_state;
    std::vector<std::shared_ptr<Node>> m_roots;

    /*
     * Swap two subtrees in the forest.
     * Return the nodes that need to be rehashed.
     */
    virtual std::shared_ptr<Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) = 0;

    /*
     * mergeRoot and newLeaf only have the desired effect if called correctly.
     * newLeaf should be called to allocate a new leaf.
     * After calling newLeaf, mergeRoot should be called for every consecutive least significant bit that is set to 1.
     */

    // Return the result of the latest merge.
    virtual std::shared_ptr<Accumulator::Node> MergeRoot(uint64_t parent_pos, uint256 parent_hash) = 0;
    virtual std::shared_ptr<Accumulator::Node> NewLeaf(uint256 hash) = 0;
    virtual void FinalizeRemove(const ForestState next_state) = 0;
    virtual void Add(const std::vector<std::shared_ptr<Accumulator::Leaf>>& leaves);
    void Remove(const std::vector<uint64_t>& targets);

    void PrintRoots(const std::vector<std::shared_ptr<Accumulator::Node>>& roots) const;
};

#endif // UTREEXO_ACCUMULATOR_H
