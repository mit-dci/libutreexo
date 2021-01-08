#ifndef UTREEXO_RAMFOREST_H
#define UTREEXO_RAMFOREST_H

#include <accumulator.h>
#include <crypto/common.h>
#include <memory>
#include <nodepool.h>
#include <unordered_map>
#include <vector>

struct LeafHasher {
    size_t operator()(const uint256& hash) const { return ReadLE64(hash.begin()); }
};

class RamForest : public Accumulator
{
private:
    // A vector of hashes for each row.
    std::vector<std::vector<uint256>> m_data;

    // A map from leaf hashes to their positions.
    // This is needed for proving that leaves are included in the accumulator.
    // TODO: only use first 12 bytes of the hash.
    std::unordered_map<uint256, uint64_t, LeafHasher> m_posmap;

    /* Return the hash at a position */
    const uint256 Read(uint64_t pos) const;

    /* Swap the hashes of ranges (from, from+range) and (to, to+range). */
    void SwapRange(uint64_t from, uint64_t to, uint64_t range);

protected:
    class Node : public Accumulator::Node
    {
    public:
        uint256 m_hash;
        // TODO: yikes.
        RamForest* m_forest;

        Node() {}

        const uint256& Hash() const override;
        void ReHash() override;
        NodePtr<Accumulator::Node> Parent() const override;
    };

    // NodePool for RamForest::Nodes
    NodePool<Node>* m_nodepool;

    NodePtr<Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) override;
    NodePtr<Accumulator::Node> MergeRoot(uint64_t parent_pos, uint256 parent_hash) override;
    NodePtr<Accumulator::Node> NewLeaf(uint256& hash) override;
    void FinalizeRemove(const ForestState next_state) override;

public:
    RamForest(ForestState& state, int max_nodes) : Accumulator(state)
    {
        this->m_data = std::vector<std::vector<uint256>>();
        this->m_data.push_back(std::vector<uint256>());
        m_nodepool = new NodePool<Node>(max_nodes);
    }

    const Accumulator::BatchProof Prove(const std::vector<uint256>& targetHashes) const override;
    void Add(const std::vector<std::shared_ptr<Accumulator::Leaf>>& leaves) override;
};

#endif // UTREEXO_RAMFOREST_H
