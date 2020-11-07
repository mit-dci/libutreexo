#ifndef UTREEXO_RAMFOREST_H
#define UTREEXO_RAMFOREST_H

#include <accumulator.h>
#include <memory>
#include <vector>

class RamForest : public Accumulator
{
private:
    // for each row store pointers to the hashes
    std::vector<std::vector<uint256>> m_data;

    const uint256 Read(uint64_t pos) const;
    void SwapRange(uint64_t from, uint64_t to, uint64_t range);

protected:
    class Node : public Accumulator::Node
    {
    private:
        uint256 m_hash;
        // TODO: yikes.
        RamForest* m_forest;

    public:
        Node(RamForest* forest, uint64_t pos, uint256 h)
            : Accumulator::Node(forest->m_state, pos), m_hash(h), m_forest(forest) {}

        const uint256& Hash() const override;
        void ReHash() override;
        std::shared_ptr<Accumulator::Node> Parent() const override;
    };

    std::shared_ptr<Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) override;
    std::shared_ptr<Accumulator::Node> MergeRoot(uint64_t parent_pos, uint256 parent_hash) override;
    std::shared_ptr<Accumulator::Node> NewLeaf(uint256 hash) override;
    void FinalizeRemove(const ForestState next_state) override;

public:
    RamForest(ForestState& state) : Accumulator(state)
    {
        this->m_data = std::vector<std::vector<uint256>>();
        this->m_data.push_back(std::vector<uint256>());
    }

    const Accumulator::BatchProof Prove(const std::vector<uint64_t>& targets) const override;
    void Add(const std::vector<std::shared_ptr<Accumulator::Leaf>>& leaves) override;
};

#endif // UTREEXO_RAMFOREST_H
