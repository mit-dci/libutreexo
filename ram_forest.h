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
    void swapRange(uint64_t from, uint64_t to, uint64_t range);

protected:
    class Node : public Accumulator::Node
    {
    private:
        uint256 h;
        RamForest* forest;

    public:
        Node(RamForest* forest, uint64_t pos, uint256 h)
            : Accumulator::Node(forest->state, pos), h(h), forest(forest) {}

        const uint256& hash() const override;
        void reHash() override;
        std::shared_ptr<Accumulator::Node> parent() const override;
    };

    std::vector<std::shared_ptr<Accumulator::Node>> roots() const override;
    std::shared_ptr<Accumulator::Node> swapSubTrees(uint64_t posA, uint64_t posB) override;
    std::shared_ptr<Accumulator::Node> mergeRoot(uint64_t parentPos, uint256 parentHash) override;
    std::shared_ptr<Accumulator::Node> newLeaf(uint256 hash) override;
    void finalizeRemove(const ForestState nextState) override;

public:
    RamForest(ForestState& state) : Accumulator(state)
    {
        this->data = std::vector<std::vector<uint256>>();
        this->data.push_back(std::vector<uint256>());
    }

    const Accumulator::BatchProof prove(const std::vector<uint64_t>& targets) const override;
};

#endif // UTREEXO_RAMFOREST_H
