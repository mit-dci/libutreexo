#ifndef UTREEXO_BATCHPROOF_H
#define UTREEXO_BATCHPROOF_H

#include <stdint.h>
#include <vector>

namespace utreexo {

/** BatchProof represents a proof for multiple leaves. */
class BatchProof
{
private:
    // The positions of the leaves that are being proven.
    std::vector<uint64_t> targets;

    // The proof hashes for the targets.
    std::vector<std::array<uint8_t, 32>> proof;

public:
    BatchProof(std::vector<uint64_t> targets, std::vector<std::array<uint8_t, 32>> proof)
        : targets(targets), proof(proof) {}
    BatchProof() {}

    const std::vector<uint64_t>& GetTargets() const;

    void Serialize(std::vector<uint8_t>& bytes) const;
    bool Unserialize(const std::vector<uint8_t>& bytes);

    bool operator==(const BatchProof& other);

    void Print();
};

};     // namespace utreexo
#endif // UTREEXO_BATCHPROOF_H
