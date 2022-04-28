#ifndef UTREEXO_RAMFOREST_H
#define UTREEXO_RAMFOREST_H

#include <fstream>
#include <optional>

#include "interface.h"

namespace utreexo {

template <typename Hash>
class BatchProof;
template <typename Hash>
class UndoBatch;

class ForestState;

class RamForest : public V1Accumulator
{
private:
    // A vector of hashes for each row.
    std::vector<std::vector<Hash>> m_data;

    // RamForests implementation of Accumulator::Node.
    class Node;

    std::optional<const Hash> Read(ForestState state, uint64_t pos) const;
    std::optional<const Hash> Read(uint64_t pos) const override;
    std::vector<Hash> ReadLeafRange(uint64_t pos, uint64_t range) const override;

    /* Swap the hashes of ranges (from, from+range) and (to, to+range). */
    void SwapRange(uint64_t from, uint64_t to, uint64_t range);

    NodePtr<V1Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) override;
    NodePtr<V1Accumulator::Node> MergeRoot(uint64_t parent_pos, Hash parent_hash) override;
    NodePtr<V1Accumulator::Node> NewLeaf(const std::pair<Hash, bool>& leaf) override;
    void FinalizeRemove(uint64_t next_num_leaves) override;

    void RestoreRoots();

    /**
     * Build the UndoBatch that can be used to roll back a modification. This
     * should only be called in Modify after the deletion and before the
     * addition of new leaves.
     */
    bool BuildUndoBatch(UndoBatch<Hash>& undo, uint64_t num_adds, const std::vector<uint64_t>& targets) const;

public:
    RamForest(uint64_t num_leaves);
    ~RamForest();

    bool Verify(const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes) override;
    bool Add(const std::vector<std::pair<Hash, bool>>& leaves) override;

    bool Modify(UndoBatch<Hash>& undo,
                const std::vector<std::pair<Hash, bool>>& new_leaves,
                const std::vector<uint64_t>& targets);

    bool Undo(const UndoBatch<Hash>& undo);

    Hash GetLeaf(uint64_t pos) const;

    bool operator==(const RamForest& other);
};

};     // namespace utreexo
#endif // UTREEXO_RAMFOREST_H
