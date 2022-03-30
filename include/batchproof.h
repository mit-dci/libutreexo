#ifndef UTREEXO_BATCHPROOF_H
#define UTREEXO_BATCHPROOF_H

#include <algorithm>
#include <array>
#include <stdint.h>
#include <vector>

namespace utreexo {

/** BatchProof represents a proof for multiple leaves. */
class BatchProof
{
private:
    // The unsorted/sorted lists of leaf positions that are being proven.
    std::vector<uint64_t> m_targets, m_sorted_targets;

    // The proof hashes for the targets.
    std::vector<std::array<uint8_t, 32>> m_proof;

public:
    BatchProof(const std::vector<uint64_t>& targets, std::vector<std::array<uint8_t, 32>> proof)
        : m_targets(targets), m_sorted_targets(targets), m_proof(proof)
    {
        std::sort(m_sorted_targets.begin(), m_sorted_targets.end());
    }

    BatchProof() {}

    void SetNull()
    {
        m_targets = std::vector<uint64_t>();
        m_sorted_targets = std::vector<uint64_t>();
        m_proof = std::vector<std::array<uint8_t, 32>>();
    }

    const std::vector<uint64_t>& GetTargets() const;
    const std::vector<uint64_t>& GetSortedTargets() const;
    const std::vector<std::array<uint8_t, 32>>& GetHashes() const;

    void Serialize(std::vector<uint8_t>& bytes) const;
    bool Unserialize(const std::vector<uint8_t>& bytes);

    /**
     * Perform some simple sanity checks on a proof.
	 * - Check that the targets are sorted in ascending order with no duplicates.
	 * - Check that the number of proof hashes is not larger than the number expected hashes.
     */
    bool CheckSanity(uint64_t num_leaves) const;

    bool operator==(const BatchProof& other);

    void Print();
};

/** UndoBatch represents the data needed to undo a batch modification in the accumulator. */
class UndoBatch
{
private:
    uint64_t m_num_additions;
    std::vector<uint64_t> m_deleted_positions;
    std::vector<std::array<uint8_t, 32>> m_deleted_hashes;

public:
    UndoBatch(uint64_t num_adds,
              const std::vector<uint64_t>& deleted_positions,
              const std::vector<std::array<uint8_t, 32>>& deleted_hashes)
        : m_num_additions(num_adds),
          m_deleted_positions(deleted_positions),
          m_deleted_hashes(deleted_hashes) {}
    UndoBatch() {}

    void Serialize(std::vector<uint8_t>& bytes) const;
    bool Unserialize(const std::vector<uint8_t>& bytes);

    uint64_t GetNumAdds() const;
    const std::vector<uint64_t>& GetDeletedPositions() const;
    const std::vector<std::array<uint8_t, 32>>& GetDeletedHashes() const;

    bool operator==(const UndoBatch& other);

    void Print();
};

};     // namespace utreexo
#endif // UTREEXO_BATCHPROOF_H
