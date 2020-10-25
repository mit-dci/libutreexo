#ifndef UTREEXO_RAMFOREST_H
#define UTREEXO_RAMFOREST_H

#include <accumulator.h>
#include <memory>
#include <vector>

class RamForest : public Accumulator
{
private:
    // for each row store pointers to the hashes
    std::vector<std::vector<uint256>> data;

    const uint256 read(uint64_t pos) const;

protected:
    class Node : public Accumulator::Node
    {
    private:
        uint256 h;

    public:
        Node(const ForestState& state, uint64_t pos, uint256 h)
            : Accumulator::Node(state, pos), h(h) {}

        const uint256& hash() const override;
    };

    std::vector<std::shared_ptr<Accumulator::Node>> roots() const override;
    std::vector<std::shared_ptr<Accumulator::Node>> swapSubTrees(uint64_t posA, uint64_t posB) override;
    std::shared_ptr<Accumulator::Node> mergeRoot(uint64_t parentPos, uint256 parentHash) override;
    std::shared_ptr<Accumulator::Node> newLeaf(uint256 hash) override;

public:
    RamForest(ForestState& state) : Accumulator(state)
    {
        this->data = std::vector<std::vector<uint256>>();
        this->data.push_back(std::vector<uint256>());
    }
};

#endif // UTREEXO_RAMFOREST_H
