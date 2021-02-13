#ifndef UTREEXO_RAMFOREST_H
#define UTREEXO_RAMFOREST_H

#include <accumulator.h>
#include <crypto/common.h>
#include <unordered_map>

namespace utreexo {
struct LeafHasher {
    size_t operator()(const Hash& hash) const { return ReadLE64(hash.data()); }
};

class RamForest : public Accumulator
{
private:
    // A vector of hashes for each row.
    std::vector<std::vector<Hash>> m_data;

    // A map from leaf hashes to their positions.
    // This is needed for proving that leaves are included in the accumulator.
    // TODO: only use first 12 bytes of the hash.
    std::unordered_map<Hash, uint64_t, LeafHasher> m_posmap;

    /* Return the hash at a position */
    bool Read(Hash& hash, uint64_t pos) const;

    /* Swap the hashes of ranges (from, from+range) and (to, to+range). */
    void SwapRange(uint64_t from, uint64_t to, uint64_t range);

    class Node : public Accumulator::Node
    {
    public:
        Hash m_hash;
        // TODO: yikes.
        RamForest* m_forest;

        Node() {}

        const Hash& GetHash() const override;
        void ReHash() override;
        NodePtr<Accumulator::Node> Parent() const override;
    };

    // NodePool for RamForest::Nodes
    NodePool<Node>* m_nodepool;

    NodePtr<Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) override;
    NodePtr<Accumulator::Node> MergeRoot(uint64_t parent_pos, Hash parent_hash) override;
    NodePtr<Accumulator::Node> NewLeaf(const Leaf& leaf) override;
    void FinalizeRemove(const ForestState next_state) override;

public:
    RamForest(ForestState& state, int max_nodes) : Accumulator(state)
    {
        this->m_data = std::vector<std::vector<Hash>>();
        this->m_data.push_back(std::vector<Hash>());
        m_nodepool = new NodePool<Node>(max_nodes);
    }

    bool Prove(Accumulator::BatchProof& proof, const std::vector<Hash>& target_hashes) const override;
    void Add(const std::vector<Leaf>& leaves) override;
};

};     // namespace utreexo
#endif // UTREEXO_RAMFOREST_H
