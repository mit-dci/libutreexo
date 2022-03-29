#ifndef UTREEXO_RAMFOREST_H
#define UTREEXO_RAMFOREST_H

#include <fstream>
#include <optional>

#include "accumulator.h"

namespace utreexo {

class BatchProof;
class UndoBatch;
class ForestState;

class RamForest : public Accumulator
{
private:
    // A vector of hashes for each row.
    std::vector<std::vector<Hash>> m_data;

    // RamForests implementation of Accumulator::Node.
    class Node;

    // Path to the file in which the forest is stored.
    std::string m_file_path;
    std::fstream m_file;

    bool Restore();

    std::optional<const Hash> Read(ForestState state, uint64_t pos) const;
    std::optional<const Hash> Read(uint64_t pos) const override;
    std::vector<Hash> ReadLeafRange(uint64_t pos, uint64_t range) const override;

    /* Swap the hashes of ranges (from, from+range) and (to, to+range). */
    void SwapRange(uint64_t from, uint64_t to, uint64_t range);

    NodePtr<Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) override;
    NodePtr<Accumulator::Node> MergeRoot(uint64_t parent_pos, Hash parent_hash) override;
    NodePtr<Accumulator::Node> NewLeaf(const Leaf& leaf) override;
    void FinalizeRemove(uint64_t next_num_leaves) override;

    void RestoreRoots();

    /**
     * Build the UndoBatch that can be used to roll back a modification.
     * This should only be called in Modify after the deletion and before the addition of new leaves.
     */
    bool BuildUndoBatch(UndoBatch& undo, uint64_t num_adds, const std::vector<uint64_t>& targets) const;

public:
    RamForest(uint64_t num_leaves);
    RamForest(const std::string& file);
    ~RamForest();

    bool Verify(const BatchProof& proof, const std::vector<Hash>& target_hashes) override;
    bool Add(const std::vector<Leaf>& leaves) override;

    bool Modify(UndoBatch& undo,
                const std::vector<Leaf>& new_leaves,
                const std::vector<uint64_t>& targets);

    bool Undo(const UndoBatch& undo);

    /** Save the forest to file. */
    bool Commit();

    Hash GetLeaf(uint64_t pos) const;

    bool operator==(const RamForest& other);
};

};     // namespace utreexo
#endif // UTREEXO_RAMFOREST_H
