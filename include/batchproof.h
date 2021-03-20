#ifndef UTREEXO_BATCHPROOF_H
#define UTREEXO_BATCHPROOF_H

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

};     // namespace utreexo
#endif // UTREEXO_BATCHPROOF_H
