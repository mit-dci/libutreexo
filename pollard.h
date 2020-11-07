#ifndef UTREEXO_POLLARD_H
#define UTREEXO_POLLARD_H

#include <accumulator.h>
#include <memory>

class Pollard : public Accumulator
{
private:
    class InternalNode
    {
    public:
        uint256 m_hash;
        std::shared_ptr<InternalNode> m_nieces[2];

        InternalNode(uint256 hash) : m_hash(hash)
        {
            this->m_nieces[0] = nullptr;
            this->m_nieces[1] = nullptr;
        }

        ~InternalNode();

        void Prune();
        bool DeadEnd();
    };

    // Return the node and its sibling.
    // Point path to the parent of the node. The path to the node can be traversed in reverse order using the
    // Accumulator::Node::parent function.
    std::vector<std::shared_ptr<Pollard::InternalNode>> Read(uint64_t pos, std::shared_ptr<Accumulator::Node>& path) const;

protected:
    class Node : public Accumulator::Node
    {
    public:
        std::shared_ptr<InternalNode> m_node;

        // Store the sibling for reHash.
        // The siblings nieces are the nodes children.
        std::shared_ptr<InternalNode> m_sibling;

        Node(const ForestState& state, uint64_t pos, std::shared_ptr<InternalNode> int_node)
            : Accumulator::Node(state, pos), m_node(int_node) {}

        Node(const ForestState& state,
             uint64_t pos,
             std::shared_ptr<InternalNode> int_node,
             std::shared_ptr<Accumulator::Node> parent,
             std::shared_ptr<InternalNode> sibling)
            : Accumulator::Node(state, pos, parent), m_node(int_node), m_sibling(sibling) {}

        const uint256& Hash() const override;
        void ReHash() override;
    };

    std::shared_ptr<Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) override;
    std::shared_ptr<Accumulator::Node> MergeRoot(uint64_t parent_pos, uint256 parent_hash) override;
    std::shared_ptr<Accumulator::Node> NewLeaf(uint256 hash) override;
    void FinalizeRemove(const ForestState next_state) override;

public:
    Pollard(ForestState& state) : Accumulator(state) {}

    const Accumulator::BatchProof Prove(const std::vector<uint64_t>& targets) const
    {
        // TODO: prove does not really make sense for the pollard.
        // although you might want to prove cached leaves.
        const BatchProof proof(targets, std::vector<uint256>());
        return proof;
    }
};

#endif // UTREEXO_POLLARD_H
