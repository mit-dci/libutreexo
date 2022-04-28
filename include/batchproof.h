#ifndef UTREEXO_BATCHPROOF_H
#define UTREEXO_BATCHPROOF_H

#include <algorithm>
#include <array>
#include <stdint.h>
#include <vector>

namespace utreexo {

/** BatchProof represents a proof for multiple leaves. */
template <typename H>
class BatchProof
{
protected:
    // The unsorted/sorted lists of leaf positions that are being proven.
    std::vector<uint64_t> m_targets, m_sorted_targets;

    // The proof hashes for the targets.
    std::vector<H> m_proof;

public:
    BatchProof(const std::vector<uint64_t>& targets, std::vector<H> proof)
        : m_targets(targets), m_sorted_targets(targets), m_proof(proof)
    {
        std::sort(m_sorted_targets.begin(), m_sorted_targets.end());
    }

    BatchProof() {}

    const std::vector<uint64_t>& GetTargets() const { return m_targets; }
    const std::vector<uint64_t>& GetSortedTargets() const { return m_sorted_targets; }
    const std::vector<H>& GetHashes() const { return m_proof; }

    bool operator==(const BatchProof& other)
    {
        return m_targets.size() == other.m_targets.size() &&
               m_proof.size() == other.m_proof.size() &&
               m_targets == other.m_targets && m_proof == other.m_proof;
    }
};

/** UndoBatch represents the data needed to undo a batch modification in the accumulator. */
template <typename H>
class UndoBatch
{
private:
    uint64_t m_num_additions{0};
    uint64_t m_prev_num_leaves{0};
    std::vector<uint64_t> m_deleted_positions;
    std::vector<H> m_deleted_hashes;

public:
    UndoBatch(uint64_t num_adds,
              const std::vector<uint64_t>& deleted_positions,
              const std::vector<H>& deleted_hashes)
        : m_num_additions(num_adds),
          m_deleted_positions(deleted_positions),
          m_deleted_hashes(deleted_hashes) {}
    UndoBatch() : m_num_additions(0) {}

    uint64_t GetNumAdds() const { return m_num_additions; }
    const std::vector<uint64_t>& GetDeletedPositions() const { return m_deleted_positions; }
    const std::vector<H>& GetDeletedHashes() const { return m_deleted_hashes; }

    bool operator==(const UndoBatch& other)
    {
        return m_num_additions == other.m_num_additions &&
               m_deleted_positions.size() == other.m_deleted_positions.size() &&
               m_deleted_hashes.size() == other.m_deleted_hashes.size() &&
               m_deleted_positions == other.m_deleted_positions &&
               m_deleted_hashes == other.m_deleted_hashes;
    }
};

};     // namespace utreexo
#endif // UTREEXO_BATCHPROOF_H
