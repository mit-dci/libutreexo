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
        const uint256 hash;
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

protected:
    class Node : public Accumulator::Node
    {
    public:
        std::shared_ptr<InternalNode> node;
        Node(const ForestState& state, uint64_t pos, std::shared_ptr<InternalNode> intNode)
            : Accumulator::Node(state, pos), node(intNode) {}

        const uint256& hash() const override;
    };

    std::vector<std::shared_ptr<Accumulator::Node>> roots() const override;
    std::vector<std::shared_ptr<Accumulator::Node>> swapSubTrees(uint64_t posA, uint64_t posB) override;
    std::shared_ptr<Accumulator::Node> mergeRoot(uint64_t parentPos, uint256 parentHash) override;
    std::shared_ptr<Accumulator::Node> newLeaf(uint256 hash) override;

public:
    Pollard(ForestState& state) : Accumulator(state) {}
};

#endif // UTREEXO_POLLARD_H
