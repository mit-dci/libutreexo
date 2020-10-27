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
        uint256 hash;
        std::vector<std::shared_ptr<InternalNode>> nieces;

        InternalNode(uint256 hash) : hash(hash)
        {
            this->nieces = std::vector<std::shared_ptr<InternalNode>>();
            this->nieces.resize(2);
            this->nieces[0] = nullptr;
            this->nieces[1] = nullptr;
        }
        ~InternalNode();
        void prune();
        bool deadEnd();
    };

    std::vector<std::shared_ptr<InternalNode>> mRoots;

    // Return the node and its sibling.
    // Point path to the parent of the node. The path to the node can be traversed in reverse order using the
    // Accumulator::Node::parent function.
    std::vector<std::shared_ptr<Pollard::InternalNode>> read(uint64_t pos, std::shared_ptr<Accumulator::Node>& path) const;

protected:
    class Node : public Accumulator::Node
    {
    public:
        std::shared_ptr<InternalNode> node;

        // Store the sibling for reHash.
        // The siblings nieces are the nodes children.
        std::shared_ptr<InternalNode> sibling;

        Node(const ForestState& state, uint64_t pos, std::shared_ptr<InternalNode> intNode)
            : Accumulator::Node(state, pos), node(intNode) {}

        Node(const ForestState& state,
             uint64_t pos,
             std::shared_ptr<InternalNode> intNode,
             std::shared_ptr<Accumulator::Node> parent,
             std::shared_ptr<InternalNode> sibling)
            : Accumulator::Node(state, pos, parent), node(intNode), sibling(sibling) {}

        const uint256& hash() const override;
        void reHash() override;
    };

    std::vector<std::shared_ptr<Accumulator::Node>> roots() const override;
    std::shared_ptr<Accumulator::Node> swapSubTrees(uint64_t posA, uint64_t posB) override;
    std::shared_ptr<Accumulator::Node> mergeRoot(uint64_t parentPos, uint256 parentHash) override;
    std::shared_ptr<Accumulator::Node> newLeaf(uint256 hash) override;
    void finalizeRemove(const ForestState nextState) override;

public:
    Pollard(ForestState& state) : Accumulator(state) {}

    const Accumulator::BatchProof prove(const std::vector<uint64_t>& targets) const
    {
        // TODO: prove does not really make sense for the pollard.
        // although you might want to prove cached leaves.
        const BatchProof proof(targets, std::vector<uint256>());
        return proof;
    }
};

#endif // UTREEXO_POLLARD_H
